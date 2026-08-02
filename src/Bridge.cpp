#include "Bridge.h"
#include "Adapter.h"
#include <map>
#include <esp_gap_ble_api.h>
#include <esp_gap_bt_api.h>

/*
  More info about those magic numbers
  https://github.com/hessu/aprs-specs/blob/master/BLE-KISS-API.md
*/
#define SERVICE_UUID "00000001-ba2a-46c9-ae49-01b0961f68bb"
#define TX_UUID "00000002-ba2a-46c9-ae49-01b0961f68bb" // From the perspective of the BLE app
#define RX_UUID "00000003-ba2a-46c9-ae49-01b0961f68bb" // From the perspective of the BLE app

#define BYTE_TRANSMIT_TIME 7             // Aprox time in ms to transmit a byte at 1200 baud
#define RETRY_BTC_CONNECT_INTERVAL 15000 // Try to connect to radio Bluetooth Classic interface every x ms
const size_t RX_BUF_SIZE = 1024;         // BLE 4.2 supports up to 512. MTU is negotiated by client.

const char PREF_RADIO_NAME[] = "radioName";
const char PREF_RADIO_ADDRESS[] = "radioAddress";
const char PREF_RIG_CTRL[] = "rigCtrl";

extern char _remote_name[ESP_BT_GAP_MAX_BDNAME_LEN + 1];
extern bool _isRemoteAddressSet;

extern Adapter *adapter;

static void copyDeviceName(char *destination, size_t destinationSize, const char *source)
{
  if (destination == nullptr || destinationSize == 0)
    return;
  snprintf(destination, destinationSize, "%s", source == nullptr ? "" : source);
}

void connectToBluetooth(void *address)
{
  adapter->bridge.btSerial.connect((uint8_t *)address, 0, ESP_SPP_SEC_NONE, ESP_SPP_ROLE_MASTER);
  vTaskDelete(NULL);
}

Bridge::Bridge(String adapterName) : bleDisconnectedState(
                                         [this]
                                         { this->bleDisconnectedEnter(); },
                                         [this]
                                         { this->bleDisconnectedUpdate(); },
                                         [this]
                                         { this->bleDisconnectedExit(); }),
                                     bleConnectedState(
                                         [this]
                                         { this->bleConnectedEnter(); },
                                         [this]
                                         { this->bleConnectedUpdate(); },
                                         [this]
                                         { this->bleConnectedExit(); }),
                                     bleStateMachine(bleDisconnectedState),
                                     btcDisconnectedState(
                                         [this]
                                         { this->btcDisconnectedEnter(); },
                                         [this]
                                         { this->btcDisconnectedUpdate(); },
                                         [this]
                                         { this->btcDisconnectedExit(); }),
                                     btcConnectedState(
                                         [this]
                                         { this->btcConnectedEnter(); },
                                         [this]
                                         { this->btcConnectedUpdate(); },
                                         [this]
                                         { this->btcConnectedExit(); }),
                                     btcDiscoveryState(
                                         [this]
                                         { this->btcDiscoveryEnter(); },
                                         [this]
                                         { this->btcDiscoveryUpdate(); },
                                         [this]
                                         { this->btcDiscoveryExit(); }),
                                     btcStateMachine(btcDisconnectedState),
                                     adapterName(adapterName),
                                     cmdQueue(MAX_EXTENDED_COMMANDS_PER_WRITE),
                                     dataQueue(16)
{
  queueMutex = xSemaphoreCreateMutex();
}

bool Bridge::init()
{
  if (queueMutex == nullptr)
  {
    return false;
  }
  rxLingerUntil = millis();
  txLingerUntil = millis();

  preferences.begin(PREFERENCES_NAMESPACE, false);
  useRigControl = preferences.getBool(PREF_RIG_CTRL, true);
  preferences.end();


  bool ok = initBTC();
  if (ok)
  {
    lookUpLastPairedDevice();
  }

  return (initBLE() && ok);
}

void Bridge::perform()
{
  bleStateMachine.update();
  btcStateMachine.update();

  uint8_t rxBuf[RX_BUF_SIZE];
  size_t rxLen = 0;

  // Commands and KISS data share a sequence across two differently-sized
  // queues. Merge the heads here so frequency changes and packet data retain
  // their original BLE write order without allocating command-sized 512-byte
  // queue entries.
  while (true)
  {
    if (!lockQueues())
    {
      break;
    }

    if (cmdQueue.isEmpty() && dataQueue.isEmpty())
    {
      unlockQueues();
      break;
    }

    const bool processCommand = !cmdQueue.isEmpty() &&
      (dataQueue.isEmpty() || cmdQueue.getHeadPtr()->sequence < dataQueue.getHeadPtr()->sequence);

    if (processCommand)
    {
      processingCmdQueue = true;
      queued_command_t queued = cmdQueue.dequeue();
      unlockQueues();
      processExtendedHardwareCommand(&queued.command);
      processingCmdQueue = false;
      continue;
    }

    ble_data_chunk_t chunk = dataQueue.dequeue();
    unlockQueues();
    if (!btcStateMachine.isInState(btcConnectedState))
    {
      continue;
    }
    btSerial.write(chunk.data, chunk.size);
    setTxLinger(BYTE_TRANSMIT_TIME * chunk.size);
  }

  if (isReady())
  {
    // Buffer data available from BTC
    const size_t notificationPayload = mtuSize > 3 ? min(static_cast<size_t>(mtuSize - 3), RX_BUF_SIZE) : 20;
    while (btSerial.available() && rxLen < notificationPayload)
    {
      rxBuf[rxLen++] = btSerial.read();
    }
    // Send data to BLE
    if (rxLen > 0)
    {
      setRxLinger(BYTE_TRANSMIT_TIME * rxLen);
      pRx->setValue(rxBuf, rxLen);
      pRx->notify();
    }
  }
}

bool Bridge::lockQueues()
{
  return queueMutex != nullptr && xSemaphoreTake(queueMutex, portMAX_DELAY) == pdTRUE;
}

void Bridge::unlockQueues()
{
  xSemaphoreGive(queueMutex);
}

void Bridge::disconnect()
{
  clearAllPendingBTCData();
  connectToPairedDevice = false;
  btSerial.disconnect();
}

void Bridge::reconnectRadio()
{
  if (remoteName[0] == '\0')
  {
    return;
  }

  clearAllPendingBTCData();
  connectToPairedDevice = true;
  btSerial.disconnect();
  btcStateMachine.immediateTransitionTo(btcDisconnectedState);
}

void Bridge::factoryReset()
{
  disconnect();
  clearPairedDevices();
  preferences.begin(PREFERENCES_NAMESPACE, false);
  preferences.clear();
  preferences.end();
  useRigControl = true;
  connectToPairedDevice = false;

  // Restart the device
  esp_restart();
}

String Bridge::getAdapterName()
{
  return adapterName;
}

bool Bridge::isReady()
{
  return (bleStateMachine.isInState(bleConnectedState) && btcStateMachine.isInState(btcConnectedState));
}

bool Bridge::btcConnected()
{
  return (btcStateMachine.isInState(btcConnectedState));
}

bool Bridge::btcDiscovery()
{
  return (btcStateMachine.isInState(btcDiscoveryState));
}

bool Bridge::initBTC()
{
  btSerial.enableSSP();
  btSerial.setPin("0000", 4);

  btSerial.onConfirmRequest([this](uint32_t num)
                            { this->onBTConfirmRequestCallback(num); });

  btSerial.onAuthComplete([this](bool success)
                          { this->onBTAuthCompleteCallback(success); });

  if (!btSerial.begin(adapterName, true))
  {
    return false;
  }
  else
  {
    configureBluetoothPower();
    return true;
  }
}

bool Bridge::isTx()
{
  return (txLingerUntil > millis());
}

bool Bridge::isRx()
{
  return (rxLingerUntil > millis());
}

bool Bridge::initBLE()
{

  BLEDevice::init(adapterName.c_str());
  configureBluetoothPower();
  pBLEServer = BLEDevice::createServer();
  if (pBLEServer == nullptr)
  {
    return false;
  }
  pBLEServer->setCallbacks(this);

  BLEService *pService = pBLEServer->createService(SERVICE_UUID);
  if (pService == nullptr)
  {
    return false;
  }

  pTx = pService->createCharacteristic(
      TX_UUID,
      BLECharacteristic::PROPERTY_WRITE_NR);

  pRx = pService->createCharacteristic(
      RX_UUID,
      BLECharacteristic::PROPERTY_NOTIFY);

  if (pTx == nullptr || pRx == nullptr)
  {
    return false;
  }

  pRx->addDescriptor(new BLE2902());

  pRx->setAccessPermissions(ESP_GATT_PERM_READ);
  pTx->setAccessPermissions(ESP_GATT_PERM_WRITE);

  pTx->setCallbacks(this);
  pRx->setCallbacks(this);

  pService->start();

  return true;
}

void Bridge::configureBluetoothPower()
{
#if BB_LINK_MOBILE_POWER_PROFILE
  esp_bredr_tx_power_set(ESP_PWR_LVL_N0, ESP_PWR_LVL_P3);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_N0);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_N0);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_CONN_HDL0, ESP_PWR_LVL_N0);
#endif
}

BLEServer *Bridge::getBLEServer()
{
  return pBLEServer;
}

void Bridge::startAdvertisingBLE()
{
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
  BLEDevice::startAdvertising();
}

void Bridge::stopAdvertisingBLE()
{
  BLEDevice::stopAdvertising();
}

void Bridge::clearAllPendingBTCData()
{
  // Purge anything that may be pending
  if (btSerial.connected())
  {
    btSerial.flush();
    while (btSerial.available())
    {
      btSerial.read();
    }
  }
}

void Bridge::lookUpLastPairedDevice()
{
  int count = esp_bt_gap_get_bond_device_num();

  clearRemoteDeviceInfo();

  if (!count)
  {
    clearStoredPairedDeviceInfo();
  }
  else
  {
    if (PAIR_MAX_DEVICES < count)
    {
      count = PAIR_MAX_DEVICES;
    }

    esp_err_t tError = esp_bt_gap_get_bond_device_list(&count, pairedDeviceBtAddr);

    if (ESP_OK == tError)
    {
      for (int i = 0; i < count; i++)
      {

        // Normally there should only be one paired device at a time. Check the address matches
        // what was stored in preferences.
        preferences.begin(PREFERENCES_NAMESPACE, false);
        String radioName = preferences.getString(PREF_RADIO_NAME);
        u_int8_t radioAddress[ESP_BD_ADDR_LEN];
        preferences.getBytes(PREF_RADIO_ADDRESS, radioAddress, ESP_BD_ADDR_LEN);
        preferences.end();

        // Check if address match
        if (BTAddress(radioAddress).equals(BTAddress(pairedDeviceBtAddr[i])))
        {
          memcpy(remoteAddress, pairedDeviceBtAddr[i], ESP_BD_ADDR_LEN);
          copyDeviceName(remoteName, sizeof(remoteName), radioName.c_str());
          connectToPairedDevice = true;
          break;
        }
        else
        {
          clearPairedDevices();
          break;
        }
      }
    }
  }
}

void Bridge::clearStoredPairedDeviceInfo()
{
  preferences.begin(PREFERENCES_NAMESPACE, false);
  preferences.remove(PREF_RADIO_NAME);
  preferences.remove(PREF_RADIO_ADDRESS);
  preferences.end();
}

void Bridge::clearRemoteDeviceInfo()
{
  remoteName[0] = '\0';
  memset(remoteAddress, 0, ESP_BD_ADDR_LEN);
  connectToPairedDevice = false;
}

void Bridge::clearPairedDevices()
{
  int count = esp_bt_gap_get_bond_device_num();

  clearStoredPairedDeviceInfo();
  clearRemoteDeviceInfo();


  if (!count)
  {
  }
  else
  {
    if (PAIR_MAX_DEVICES < count)
    {
      count = PAIR_MAX_DEVICES;
    }

    esp_err_t tError = esp_bt_gap_get_bond_device_list(&count, pairedDeviceBtAddr);

    if (ESP_OK == tError)
    {
      for (int i = 0; i < count; i++)
      {

        esp_err_t tError = esp_bt_gap_remove_bond_device(pairedDeviceBtAddr[i]);
        if (ESP_OK == tError)
        {
        }
        else
        {
        }
      }
    }
  }
}

void Bridge::setTxLinger(int linger)
{
  unsigned long now = millis();
  if (txLingerUntil < now)
  {
    txLingerUntil = now;
  }
  txLingerUntil += linger;
}

void Bridge::setRxLinger(int linger)
{
  unsigned long now = millis();
  if (rxLingerUntil < now)
  {
    rxLingerUntil = now;
  }
  rxLingerUntil += linger;
}

void Bridge::processExtendedHardwareCommand(extended_hw_cmd_t *cmd)
{
  switch (cmd->action)
  {
  case extended_hw_set_frequency:
  {
    if (useRigControl)
    {
      previousFrequency = 0;

      if (vfo != vfoUnknown)
      {
        // At this point, we should always be in KISS mode. Exit KISS mode first so we don't have to wait for a timeout
        thd7x.exitKISS();

        // Just to be sure
        if (thd7x.isKISSMode())
        {
          thd7x.exitKISS();
        }

        if (thd7x.getBaudRate(&previousBaudRate))
        {
        }

        if (desiredBaudRate != baudRateUnknown && previousBaudRate != desiredBaudRate)
        {
          thd7x.setBaudRate(desiredBaudRate);
        }

        if (thd7x.getMode(vfo, &previousMode))
        {
          if (previousMode != modeFM)
          {
            thd7x.setMode(vfo, modeFM);
          }
        }
        else
        {
          previousMode = modeUnknown;
        }


        if (thd7x.getFrequency(vfo, &previousFrequency))
        {
          thd7x.setFrequency(vfo, cmd->data.uint32);
        }
        else
        {
          previousFrequency = 0;
        }

        // Show time
        thd7x.setTNC(vfo, tncKISS);
      }
      else
      {
      }
    }
    break;
  }
  case extended_hw_restore_frequency:
  {
    if (useRigControl)
    {

      if (vfo != vfoUnknown && previousFrequency > 0)
      {
        // Always exit KISS mode first so we don't have to wait for a timeout
        thd7x.exitKISS();

        // Just to be sure
        if (thd7x.isKISSMode())
        {
          thd7x.exitKISS();
        }

        thd7x.setFrequency(vfo, previousFrequency);
        previousFrequency = 0;

        if (previousMode != modeUnknown && previousMode != modeFM)
        {
          thd7x.setMode(vfo, previousMode);
        }

        if (desiredBaudRate != baudRateUnknown && previousBaudRate != desiredBaudRate)
        {
          thd7x.setBaudRate(previousBaudRate);
        }

        // As long as BLE is connected, we want the radio to be in KISS mode
        thd7x.setTNC(vfo, tncKISS);
      }
      else
      {
      }
    }
    break;
  }
  case extended_hw_set_baud_rate:
  {
    /*
      Save the desired baud rate for now. We could set the radio right away
      but that would require getting out of KISS mode which is an expensive operation
      and leads to the radio beeping. It's also expected to be seldome as there are not
      many 9600 stations.
      Defeer setting the baud rate when changing frequency. This assumes that the host 
      application sets the baud rate first.
    */
    desiredBaudRate = static_cast<baud_rate_t>(cmd->data.uint8);
    break;
  }
  case extended_hw_start_scan:
  {
    btcStateMachine.transitionTo(btcDiscoveryState);
    break;
  }
  case extended_hw_stop_scan:
  {
    btcStateMachine.transitionTo(btcDisconnectedState);
    break;
  }
  case extended_hw_pair_with_device:
  {
    btSerial.disconnect();
    clearPairedDevices();

    // iterate thru the list of devices found during scan
    // check if the device address is in the list and get its name
    btDeviceList = btSerial.getScanResults();
    for (int i = 0; i < btDeviceList->getCount(); i++)
    {
      BTAdvertisedDevice *device = btDeviceList->getDevice(i);
      if (device->getAddress().equals(BTAddress(cmd->data.bytes)))
      {
        memcpy(remoteAddress, device->getAddress().getNative(), sizeof(esp_bd_addr_t));
        copyDeviceName(remoteName, sizeof(remoteName), device->getName().c_str());
        connectToPairedDevice = true;
        // force connections
        btcStateMachine.immediateTransitionTo(btcDisconnectedState);
        break;
      }
    }
    break;
  }
  case extended_hw_clear_paired_device:
  {
    btSerial.disconnect();
    clearPairedDevices();
    break;
  }
  case extended_hw_api_version:
  {
    reply16(EXTENDED_HW_CMD_API_VERSION, API_VERSION);
    break;
  }
  case extended_hw_firmware_version:
  {
    // create period delimited version string
    String version = String(FIRMWARE_VERSION_MAJOR) + "." + String(FIRMWARE_VERSION_MINOR) + "." + String(FIRMWARE_VERSION_PATCH);
    reply(EXTENDED_HW_CMD_FIRMWARE_VERSION, (uint8_t *)version.c_str(), strlen(version.c_str()));
    break;
  }
  case extended_hw_capabilities:
  {
    uint16_t caps;
    caps = (useRigControl ? CAP_RIG_CTRL : 0) | CAP_FIRMWARE_VERSION;
    reply16(EXTENDED_HW_CMD_CAPABILITIES, caps);
    break;
  }
  case extended_hw_get_paired_device:
  {
    found_device_t paired;
    paired.connected = btcConnected() ? 0x01 : 0x00;
    memcpy(paired.address, remoteAddress, sizeof(esp_bd_addr_t));
    copyDeviceName(paired.name, sizeof(paired.name), remoteName);
    reply(EXTENDED_HW_CMD_GET_PAIRED_DEVICE, reinterpret_cast<uint8_t *>(&paired), 1 + sizeof(esp_bd_addr_t) + strlen(paired.name));
    break;
  }
  case extended_hw_set_rig_ctrl:
  {
    if (cmd->data.uint8 == 0x00)
    {
      useRigControl = false;
    }
    else
    {
      useRigControl = true;
    }
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.putBool(PREF_RIG_CTRL, useRigControl);
    preferences.end();
    break;
  }
  case extended_hw_factory_reset:
  {
    factoryReset();
    break;
  }
  default:
    break;
  }
}

void Bridge::reply8(uint8_t cmd, uint8_t data)
{
  uint8_t buffer[3] = {CMD_HARDWARE, cmd, data};
  reply(buffer, 3);
}

void Bridge::reply16(uint8_t cmd, uint16_t data)
{
  // Big endian
  uint8_t buffer[4] = {CMD_HARDWARE, cmd, uint8_t((data >> 8) & 0xFF), uint8_t(data & 0xFF)};
  reply(buffer, 4);
}

void Bridge::reply(uint8_t cmd, uint8_t *data, size_t size)
{
  uint8_t buffer[MAX_KISS_FRAME_SIZE];
  if (size + 2 > sizeof(buffer))
  {
    return;
  }
  buffer[0] = CMD_HARDWARE;
  buffer[1] = cmd;
  memcpy(buffer + 2, data, size);
  reply(buffer, size + 2);
}

void Bridge::reply(uint8_t *response, size_t size)
{
  uint8_t buffer[MAX_KISS_FRAME_SIZE * 2 + 2];
  if (size > MAX_KISS_FRAME_SIZE)
  {
    return;
  }
  size_t bufferSize = sizeof(buffer);

  if (kissInterceptor.escape(response, size, buffer, &bufferSize))
  {
    pRx->setValue(buffer, bufferSize);
    pRx->notify();
  }
  else
  {
  }
}

/*
  BLEServerCallbacks
*/
void Bridge::onConnect(BLEServer *pServer, esp_ble_gatts_cb_param_t *param)
{

  esp_ble_conn_update_params_t conn_params = {};
  memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
  conn_params.latency = 0;
  conn_params.max_int = 32;  // *1.25ms = 40ms
  conn_params.min_int = 16;  // *1.25ms = 20ms
  conn_params.timeout = 500; // *10ms = 5000ms
  // Start the update process
  esp_ble_gap_update_conn_params(&conn_params);

  bleStateMachine.transitionTo(bleConnectedState);
}

void Bridge::onDisconnect(BLEServer *pServer)
{
  bleStateMachine.transitionTo(bleDisconnectedState);
}

void Bridge::onMtuChanged(BLEServer *pServer, esp_ble_gatts_cb_param_t *param)
{
  mtuSize = max(static_cast<uint16_t>(23), param->mtu.mtu);
}

/*
  BLECharacteristicCallbacks
*/
void Bridge::onWrite(BLECharacteristic *pCharacteristic)
{
  const size_t txSize = pCharacteristic->getLength();

  if (txSize > 0)
  {

    size_t eventCount = 0;
    size_t passthroughSize = 0;
    kiss_process_result_t result = kissInterceptor.process(
      reinterpret_cast<const uint8_t *>(pCharacteristic->getData()),
      txSize,
      parsedEvents,
      MAX_KISS_EVENTS_PER_WRITE,
      &eventCount,
      kissPassthrough,
      sizeof(kissPassthrough),
      &passthroughSize);

    if (result != kiss_process_ok)
    {
      kissInterceptor.reset();
      return;
    }

    size_t commandCount = 0;
    size_t dataChunksRequired = 0;
    for (size_t i = 0; i < eventCount; ++i)
    {
      if (parsedEvents[i].type == kiss_output_command)
      {
        commandCount++;
      }
      else
      {
        dataChunksRequired += (parsedEvents[i].size + MAX_BLE_WRITE_SIZE - 1) / MAX_BLE_WRITE_SIZE;
      }
    }

    if (commandCount == 0)
    {
      queueOrSendBLEData(kissPassthrough, passthroughSize);
      return;
    }

    if (!lockQueues())
    {
      return;
    }

    const size_t commandSlots = cmdQueue.maxQueueSize() - cmdQueue.itemCount();
    if (commandCount > commandSlots)
    {
      unlockQueues();
      disconnect();
      kissInterceptor.reset();
      return;
    }

    const size_t chunkSlots = dataQueue.maxQueueSize() - dataQueue.itemCount();
    if (dataChunksRequired > chunkSlots)
    {
      unlockQueues();
      disconnect();
      kissInterceptor.reset();
      return;
    }

    for (size_t i = 0; i < eventCount; ++i)
    {
      if (parsedEvents[i].type == kiss_output_command)
      {
        queued_command_t queued = { nextQueueSequence++, parsedEvents[i].command };
        if (!cmdQueue.enqueue(queued))
        {
          unlockQueues();
          disconnect();
          kissInterceptor.reset();
          return;
        }
      }
      else if (!queueBLEDataLocked(kissPassthrough + parsedEvents[i].offset, parsedEvents[i].size))
      {
        unlockQueues();
        disconnect();
        kissInterceptor.reset();
        return;
      }
    }
    unlockQueues();
  }
}

bool Bridge::queueBLEDataLocked(const uint8_t *data, size_t size)
{
  size_t offset = 0;
  while (offset < size)
  {
    ble_data_chunk_t chunk = {};
    chunk.sequence = nextQueueSequence++;
    chunk.size = min(size - offset, sizeof(chunk.data));
    memcpy(chunk.data, data + offset, chunk.size);
    if (!dataQueue.enqueue(chunk))
    {
      return false;
    }
    offset += chunk.size;
  }
  return true;
}

void Bridge::queueOrSendBLEData(const uint8_t *data, size_t size)
{
  if (data == nullptr || size == 0)
    return;

  if (!lockQueues())
  {
    return;
  }

  const bool sendImmediately = btcStateMachine.isInState(btcConnectedState) &&
    !processingCmdQueue && cmdQueue.isEmpty() && dataQueue.isEmpty();
  if (sendImmediately)
  {
    unlockQueues();
    btSerial.write(data, size);
    setTxLinger(BYTE_TRANSMIT_TIME * size);
    return;
  }

  const size_t chunksRequired = (size + MAX_BLE_WRITE_SIZE - 1) / MAX_BLE_WRITE_SIZE;
  const size_t chunkSlots = dataQueue.maxQueueSize() - dataQueue.itemCount();
  if (chunksRequired > chunkSlots)
  {
    unlockQueues();
    disconnect();
    return;
  }

  if (!queueBLEDataLocked(data, size))
  {
    unlockQueues();
    disconnect();
    return;
  }
  unlockQueues();
}

void Bridge::onRead(BLECharacteristic *pCharacteristic)
{
}

/*
  BTC callbacks
*/
void Bridge::onBTConfirmRequestCallback(uint32_t numVal)
{
  btSerial.confirmReply(true);
}

void Bridge::onBTAuthCompleteCallback(boolean success)
{
  if (success)
  {
    /*
      Save the name and address of the radio we just paired
      This so we can connect to it next time we start the device and
      display its name in the configuration app
    */
    preferences.begin(PREFERENCES_NAMESPACE, false);
    preferences.putString(PREF_RADIO_NAME, remoteName);
    preferences.putBytes(PREF_RADIO_ADDRESS, remoteAddress, ESP_BD_ADDR_LEN);
    preferences.end();
  }
  else
  {
  }
}

/*
  BTC states
*/
void Bridge::btcDisconnectedEnter()
{
  if (connectToPairedDevice)
  {
    // The connect method in BT serial is blocking. Use a task to connect
    btSerial.disconnect(); // Just in case. If radio is already connected, reconnecting could lead to crash
    xTaskCreate(
        connectToBluetooth,    // Task function
        "connectBT",           // Task name
        4096,                  // Stack size
        (void *)remoteAddress, // Task input parameter
        1,                     // Priority of the task
        NULL                   // Task handle
    );
  }
}

void Bridge::btcDisconnectedUpdate()
{
  if (btSerial.connected())
  {
    btcStateMachine.transitionTo(btcConnectedState);
  }

  if (btcStateMachine.timeInCurrentState() > RETRY_BTC_CONNECT_INTERVAL)
  {
    // Retry connection
    btcStateMachine.immediateTransitionTo(btcDisconnectedState);
  }
}

void Bridge::btcDisconnectedExit()
{
}

void Bridge::btcConnectedEnter()
{
  clearAllPendingBTCData();
  if (bleStateMachine.isInState(bleConnectedState))
  {
    configureRadioForBLESession();
  }
}

void Bridge::btcConnectedUpdate()
{
  if (!btSerial.connected())
  {
    btcStateMachine.transitionTo(btcDisconnectedState);
  }
}

void Bridge::btcConnectedExit()
{
}

void Bridge::btcDiscoveryEnter()
{
  btSerial.disconnect();

  btDeviceList = btSerial.getScanResults();
  if (btSerial.discoverAsync([this](BTAdvertisedDevice *pDevice)
                             {
    /*
    Name: TH-D74, Address: 04:ee:03:61:2d:b0, cod: 0x620204, rssi: -55
    Name: TNC4 Mobilinkd, Address: 34:81:f4:aa:b2:dd, cod: 0x4c0300, rssi: -24
    Name: PicoAPRS, Address: 4c:75:25:65:29:82, cod: 0x001f00, rssi: -64
    Name: TH-D75, Address: 40:79:12:e4:65:44, cod: 0x620204, rssi: -62
    */
    // Filter list to known Kenwood handsets capabilities signature
    // https://www.ampedrftech.com/cod.htm?result=620204
    if (pDevice->getCOD() == 0x620204)
    {
      found_device_t found;
      found.connected = 0x00;
      memcpy(found.address, pDevice->getAddress().getNative(), sizeof(esp_bd_addr_t));
      copyDeviceName(found.name, sizeof(found.name), pDevice->getName().c_str());
      reply(EXTENDED_HW_CMD_FOUND_DEVICE, reinterpret_cast<uint8_t *>(&found), 1 + sizeof(esp_bd_addr_t) + strlen(found.name));
    } }))
  {
  }
  else
  {
  }
}

void Bridge::btcDiscoveryUpdate()
{
}

void Bridge::btcDiscoveryExit()
{
  btSerial.discoverAsyncStop();
  // Give time for the scan to stop and seems that sometimes the device
  // names don't come thru
  delay(500);
}

/*
  BLE states
*/

void Bridge::bleDisconnectedEnter()
{
  kissInterceptor.reset();
  if (lockQueues())
  {
    while (!cmdQueue.isEmpty()) cmdQueue.dequeue();
    while (!dataQueue.isEmpty()) dataQueue.dequeue();
    processingCmdQueue = false;
    unlockQueues();
  }
  startAdvertisingBLE();
}

void Bridge::bleDisconnectedUpdate()
{
}

void Bridge::bleDisconnectedExit()
{
  stopAdvertisingBLE();
}

void Bridge::bleConnectedEnter()
{
  clearAllPendingBTCData();

  configureRadioForBLESession();
}

void Bridge::configureRadioForBLESession()
{

  if (useRigControl)
  {
    vfo = vfoUnknown;
    previousTNCMode = tncUnknown;

    // Make sure there is a radio to talk to. This helper is called from both
    // BLE and Bluetooth Classic connect paths, so reconnecting either side
    // always re-establishes KISS mode.
    if (btSerial.connected())
    {
      /*
      * We don't know what TNC mode the radio is in. If the KISS TNC is already on,
      * we can't send any commands to the radio, so we have to exit it first.
      */
      if (thd7x.isKISSMode())
      {
        previousTNCMode = tncKISS;
        thd7x.exitKISS();
      }

      // Figure out which VFO is active for KISS mode
      tnc_mode_t mode;
      if (thd7x.getTNC(&vfo, &mode))
      {

        if (previousTNCMode == tncUnknown)
        {
          previousTNCMode = mode;
        }

        // As long as BLE is connected, we want the radio to be in KISS mode so application
        // can send data to the radio
        thd7x.setTNC(vfo, tncKISS);
      }
      else
      {
        vfo = vfoUnknown;
        previousTNCMode = tncUnknown;
      }
    }
  }
}

void Bridge::bleConnectedUpdate()
{
}

void Bridge::bleConnectedExit()
{
  if (useRigControl)
  {
    // Make sure there is a radio to talk to
    if (btcStateMachine.isInState(btcConnectedState))
    {
      if (previousTNCMode != tncKISS && previousTNCMode != tncUnknown && vfo != vfoUnknown)
      {

        // Always exit KISS mode first so we don't have to wait for a timeout
        thd7x.exitKISS();

        // Just to be sure
        if (thd7x.isKISSMode())
        {
          thd7x.exitKISS();
        }

        thd7x.setTNC(vfo, previousTNCMode);
      }
    }
  }
}
