# Using High-level AzureSphere App with Ultrasonic Sensor to send telemetry data to Azure IoT Hub

In this tutorial, we will build high-level AzureSphere App that connects Ultrasonic sensor and sends telemetry data to Azure IoT Hub. We will use Device Provisioning Service method to Authenticate Device with IoT Hub and we will use analog-to-digital converter
(ADC) to collect telemetry from the sensor

## Hardware Used
1. Seeed [MT3620](https://www.seeedstudio.com/Azure-Sphere-MT3620-Development-Kit-US-Version-p-3052.html) Azure Sphere Board
2. Sonar Sensor-[LV-MaxSonar-EZ1 Ultrasonic Range Finder](https://www.amazon.com/Maxbotix-MB1010-LV-MaxSonar-EZ1-Ultrasonic-Finder/dp/B00A7YGVJI)
3. Visual Studio Code with number of extensions. Please check this [guide](https://docs.microsoft.com/en-us/azure/iot-hub/iot-hub-arduino-iot-devkit-az3166-get-started) for additional details

##  Azure Sphere - Quick Getting Started
1. You'll need Azure Account and a small budget to run IoT Hub. (device provisioning service charges $0.123 per 1000 operations)
2. Install the [Azure Sphere SDK](https://aka.ms/AzureSphereSDKDownload/Windows)
3. Install [USB Serial Converters](https://docs.microsoft.com/en-us/azure-sphere/install/install-sdk?pivots=visual-studio)
4. Visual Studio Code 
   - For VS Code, Install [CMake](https://cmake.org/download/) and [Ninja on Windows](https://github.com/ninja-build/ninja/releases)
   - Add the CMake bin directory and the directory containing ninja.exe to your PATH
   - Add VS Code extension:  Azure Sphere
5. Claim your device. First time user: [click for more details](https://docs.microsoft.com/en-us/azure-sphere/install/claim-device)
   - ```azsphere login --newuser <email-address>```
   - Connect an Azure Sphere device to your computer by USB.
   - Create tenant:  ```azsphere tenant create --name <my-tenant>```
   - ```azsphere device claim```    ***Important:*** device can only be claimed once. Please check this [article](https://docs.microsoft.com/en-us/azure-sphere/install/claim-device) for more details

## Configure Device
1. Configure Network: ```azsphere device wifi add --ssid <NAME> --psk <PASS>```
2. Test connection: ```azsphere device wifi show-status```


##  Wiring Diagram
![](http://borisbrodsky.com/wp-content/uploads/2020/11/mt3620_sonar-e1606270041641.png)

### Configure Azure IoT Hub
Create an IoT Hub and a device provisioning service. Follow stpes 1-5 in this [tutorial](https://docs.microsoft.com/en-us/learn/modules/develop-secure-iot-solutions-azure-sphere-iot-hub/8-exercise-connect-room-environment-monitor) 

###  Code Configuration
1. Update app_manifest.json: 
    - ```"CmdArgs": [ "--ConnectionType", "DPS", "--ScopeID", "<scope id>" ]```
    - Run: ```azsphere tenant show-selected``` Update:  ``` "DeviceAuthentication": "<Default Tenant>"```
    - ``` "AllowedConnections": [ "global.azure-devices-provisioning.net", "<IOT HUB HOST>"]```
2. Update main.c file:
   - static char *scopeID = "xxxx";
   - static char *hostName ="xxx";
3. Validate that libraries are refferenced at CMakeList.txt file:
   - add_executable(${PROJECT_NAME} main.c eventloop_timer_utilities.c)
   - target_include_directories(${PROJECT_NAME} PUBLIC ${AZURE_SPHERE_API_SET_DIR}/usr/include/azureiot 
                           ${AZURE_SPHERE_API_SET_DIR}/usr/include/azure_prov_client 
                           ${AZURE_SPHERE_API_SET_DIR}/usr/include/azure_c_shared_utility)
                           
                          
 Hit ***F5*** in VS Code to run this project on the board
 
 
 ### Additional Materials
 * Overview of [azsphere](https://docs.microsoft.com/en-us/azure-sphere/reference/overview)
 * Overview of [Cloud Deployment](https://docs.microsoft.com/en-us/azure-sphere/install/qs-first-deployment)
 * Available [Hardware definitions](https://github.com/Azure/azure-sphere-samples/tree/master/HardwareDefinitions)(compatable boards)

