
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <applibs/adc.h>
#include <signal.h>
#include <applibs/log.h>



#include <applibs/eventloop.h>
#include <applibs/networking.h>


// Azure IoT SDK
#include <iothub_client_core_common.h>
#include <iothub_device_client_ll.h>
#include <iothub_client_options.h>
#include <iothubtransportmqtt.h>
#include <iothub.h>
#include <azure_sphere_provisioning.h>
#include <iothub_security_factory.h>
#include <shared_util_options.h>



#include <hw/template_appliance.h>
#include "eventloop_timer_utilities.h"


/////// Azure IoT definitions.
static char *scopeID = "xxxx";
static char *hostName ="xxx";

static const char networkInterface[] = "wlan0";
static volatile sig_atomic_t terminationSignal = false;
static int adcControllerFd = -1;
static EventLoop *eventLoop = NULL;
static EventLoopTimer *azureTimer = NULL;
static EventLoopTimer *deviceEventTimer = NULL;
static bool connectedToIoTHub = false;
static IOTHUB_DEVICE_CLIENT_LL_HANDLE iothubClientHandle = NULL;
static int iotHubClientAuthenticated = 0;


static int telemetryCount = 0;


static void SonarADCTimerEventHandler(EventLoopTimer *timer);
static void AzureTimerEventHandler(EventLoopTimer *timer);
static void SendTelemetry(const char *jsonMessage);
static void initAZureAndInput(void);
static void SendEventCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *context);
static bool SetUpAzureIoTHubClientWithDps(void);



/// <summary>
///     Signal handler for termination requests. This handler must be async-signal-safe.
/// </summary>
static void TerminationHandler(int signalNumber)
{
    // Don't use Log_Debug here, as it is not guaranteed to be async-signal-safe.
   terminationSignal = true;
}




/// <summary>
///    init ADC input for controller
/// </summary>
static void  initAZureAndInput(){
    adcControllerFd = ADC_Open(MT3620_ADC_CONTROLLER0);
    if (adcControllerFd == -1) {
        Log_Debug("ADC_Open failed with error: %s (%d)\n", strerror(errno), errno);
       
    }
 
}


/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
    DisposeEventLoopTimer(deviceEventTimer);
    DisposeEventLoopTimer(azureTimer);
    EventLoop_Close(eventLoop);

    Log_Debug("INFO: Closing file descriptors\n");

}

//---------------------------------------- AZURE FUNCTIONS ----------------------------------------------------------

/// <summary>
///     Sets up the Azure IoT Hub connection (creates the iothubClientHandle)
///     with DPS
/// </summary>
static bool SetUpAzureIoTHubClientWithDps(void)
{
    AZURE_SPHERE_PROV_RETURN_VALUE provResult =
        IoTHubDeviceClient_LL_CreateWithAzureSphereDeviceAuthProvisioning(scopeID, 10000,
                                                                          &iothubClientHandle);
    Log_Debug("IoTHubDeviceClient_LL_CreateWithAzureSphereDeviceAuthProvisioning returned '%s'.\n",
             provResult.result);

    if (provResult.result != AZURE_SPHERE_PROV_RESULT_OK) {
        return false;
    }

    return true;
}


/// <summary>
///     Sets up the Azure IoT Hub connection (creates the iothubClientHandle)
///     When the SAS Token for a device expires the connection needs to be recreated
///     which is why this is not simply a one time call.
/// </summary>
static void SetUpAzureIoTHubClient(void)
{
    bool isClientSetupSuccessful = false;

    if (iothubClientHandle != NULL) {
        IoTHubDeviceClient_LL_Destroy(iothubClientHandle);
    }

     //FOR DPS
    isClientSetupSuccessful = SetUpAzureIoTHubClientWithDps();


    if (!isClientSetupSuccessful) {

        Log_Debug("ERROR: Failed to create IoTHub Handle - attempting again.\n");
        return;
    }

    //set authantication flag to 1 (SUCCESS)
    iotHubClientAuthenticated =1;

                                                 
}
/// <summary>
///     Check the network status.
/// </summary>
static bool IsConnectionReadyToSendTelemetry(void)
{
    Networking_InterfaceConnectionStatus status;
    if (Networking_GetInterfaceConnectionStatus(networkInterface, &status) != 0) {
        if (errno != EAGAIN) {
            Log_Debug("ERROR: Networking_GetInterfaceConnectionStatus: %d (%s)\n", errno,
                      strerror(errno));
         
            return false;
        }
        Log_Debug(
            "WARNING: Cannot send Azure IoT Hub telemetry because the networking stack isn't ready "
            "yet.\n");
        return false;
    }

    if ((status & Networking_InterfaceConnectionStatus_ConnectedToInternet) == 0) {
        Log_Debug(
            "WARNING: Cannot send Azure IoT Hub telemetry because the device is not connected to "
            "the internet.\n");
        return false;
    }

    return true;
}


/// <summary>
///     Sends telemetry to Azure IoT Hub
/// </summary>
static void SendTelemetry(const char *jsonMessage)
{
    if (iotHubClientAuthenticated != 1) {
        // AzureIoT client is not authenticated. Log a warning and return.
        Log_Debug("WARNING: Azure IoT Hub is not authenticated. Not sending telemetry.\n");
        return;
    }

    Log_Debug("Sending Azure IoT Hub telemetry: %s.\n", jsonMessage);

    // Check whether the device is connected to the internet.
    if (IsConnectionReadyToSendTelemetry() == false) {
        return;
    }

    IOTHUB_MESSAGE_HANDLE messageHandle = IoTHubMessage_CreateFromString(jsonMessage);

    if (messageHandle == 0) {
        Log_Debug("ERROR: unable to create a new IoTHubMessage.\n");
        return;
    }

    if (IoTHubDeviceClient_LL_SendEventAsync(iothubClientHandle, messageHandle, SendEventCallback, NULL) != IOTHUB_CLIENT_OK) {
        Log_Debug("ERROR: failure requesting IoTHubClient to send telemetry event.\n");
    } else {
        Log_Debug("INFO: IoTHubClient accepted the telemetry event for delivery.\n");
   // ThreadAPI_Sleep(10);
    }

    IoTHubMessage_Destroy(messageHandle);
}






/// <summary>
///     Callback invoked when the Azure IoT Hub send event request is processed.
/// </summary>
static void SendEventCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result, void *context)
{
    Log_Debug("INFO: Azure IoT Hub send telemetry event callback: status code %d.\n", result);
}

////---------------------------------------- TIMER FUNCTIONS---------------------------------------------------------


/// <summary>
///     timer event: check ADC input voltage
/// </summary>
static void SonarADCTimerEventHandler(EventLoopTimer *timer)
{
 Log_Debug("\tINFO: Sonar timer loop\n");
         
  
    if (ConsumeEventLoopTimerEvent(timer) != 0) {
        return;
    }
    uint32_t value;
    int result = ADC_Poll(adcControllerFd, MT3620_ADC_CONTROLLER0, &value);
       if (result == -1) {
			Log_Debug("INFO: Sonar sensor reading: Not read!\n");
		}
		else {

          char tempAsString[50];
		  sprintf(tempAsString, "{\"Distance\": \"%.1f\"}", (float)value);

           //send telemetry to Azure IoT Device
	       SendTelemetry(tempAsString);
		}
}

/// <summary>
///      timer event:  Check connection status and send telemetry
/// </summary>
static void AzureTimerEventHandler(EventLoopTimer *timer)
{
    Log_Debug("\tINFO: Azure timer loop\n");

        static time_t lastTimeLogged = 0;

    if (ConsumeEventLoopTimerEvent(timer) != 0) {
     
        return;
    }

   Networking_InterfaceConnectionStatus status;

   if (Networking_GetInterfaceConnectionStatus(networkInterface, &status) == 0) {
        if ((status & Networking_InterfaceConnectionStatus_ConnectedToInternet) &&  (iotHubClientAuthenticated == 0) ) { 
            
            SetUpAzureIoTHubClient();
            
        }
    } else {
        if (errno != EAGAIN) {
            Log_Debug("ERROR: Networking_GetInterfaceConnectionStatus: %d (%s)\n", errno,
                      strerror(errno));
           
            return;
        }
    }

    


    if (iothubClientHandle != NULL) {
         Log_Debug("IoTHubDeviceClient_LL_DoWork: OK \n");
        IoTHubDeviceClient_LL_DoWork(iothubClientHandle);
    }

}




////------------------------------------------------ MAIN-------------------------------------------------------


int main(void)
{
   initAZureAndInput();

    //SIGTERM for termonation requests
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = TerminationHandler;
    sigaction(SIGTERM, &action, NULL);



    eventLoop = EventLoop_Create();
    if (eventLoop == NULL) {
        Log_Debug("Could not create event loop.\n");
    }



    struct timespec azureTelemetryPeriod = {.tv_sec = 1, .tv_nsec = 0};
    azureTimer = CreateEventLoopPeriodicTimer(eventLoop, &AzureTimerEventHandler, &azureTelemetryPeriod);
    if (azureTimer == NULL) {
            Log_Debug("Could not run event loop.\n");
    }


  // Set up a timer to poll for button events.
    static const struct timespec sonarCheckPeriod = {.tv_sec = 10, .tv_nsec = 0};
    deviceEventTimer = CreateEventLoopPeriodicTimer(eventLoop, &SonarADCTimerEventHandler,
                                                   &sonarCheckPeriod);
    if (deviceEventTimer == NULL) {
      Log_Debug("Could not run event loop.\n");
    }
    



     while (!terminationSignal) {
        EventLoop_Run_Result result = EventLoop_Run(eventLoop, -1, true);
        // Continue if interrupted by signal, e.g. due to breakpoint being set.
        if (result == EventLoop_Run_Failed && errno != EINTR) {
           
        }
      

        // Continue if interrupted by signal, e.g. due to breakpoint being set.
        if (result == EventLoop_Run_Failed && errno != EINTR) {
             Log_Debug("Loop failed\n");
        }


    }

        ClosePeripheralsAndHandlers();

}





