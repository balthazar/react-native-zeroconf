using Microsoft.ReactNative.Managed;
using Microsoft.ReactNative;
using Newtonsoft.Json;
using System;
using System.Diagnostics;
using System.Linq;
using Zeroconf;

namespace RNZeroConf
{
    [ReactModule]
    sealed class ZeroConfModule
    {
        // Member variable
        private ZeroconfResolver.ResolverListener resolverListener = null;

        // Constant
        [ReactConstant]
        private const string PROTOCOL = "_alfen._tcp.local.";

        /// <summary>
        /// React Event fired when a new Alfen Charge Station is found on the network.
        /// </summary>
        [ReactEvent("resolved")]
        public Action<string> DeviceDiscovered { get; set; }

        /// <summary>
        /// React Event fired when a CS is no longer found in the network.
        /// </summary> 
        [ReactEvent("remove")]
        public Action<string> DeviceLost { get; set; }

        /// <summary>
        /// React Event fired when an Error has occured.
        /// </summary>
        [ReactEvent("error")]
        public Action<Exception> ErrorOccured { get; set; }

        /// <summary>
        /// Constructor
        /// </summary>
        public ZeroConfModule()
        {
        }

        /// <summary>
        /// Start the Discovery of the Alfen ChargeStations
        /// </summary>
        /// <returns> True, if started succesfully.</returns>
        [ReactMethod("Start")]
        public bool StartDiscovery()
        {
            if (resolverListener == null)
            {
                Debug.WriteLine("ResolveListener Started.");
                resolverListener = ZeroconfResolver.CreateListener(PROTOCOL);
                resolverListener.ServiceFound += ResolverListener_ServiceFound;
                resolverListener.ServiceLost += ResolverListener_ServiceLost;
                resolverListener.Error += ResolverListener_Error;
                return true;
            }
            else
            {
                Debug.WriteLine("Warning: ResolveListener already Running.");
                return false;
            }
        }

        /// <summary>
        /// Stops and Disposes the Listener functionality
        /// </summary>
        /// <returns> True, if stopped succesfully.</returns>
        [ReactMethod("Stop")]
        public bool StopDiscovery()
        {
            if (resolverListener != null)
            {
                Debug.WriteLine("ResolveListener Disposed");
                resolverListener.ServiceFound -= ResolverListener_ServiceFound;
                resolverListener.ServiceLost -= ResolverListener_ServiceLost;
                resolverListener.Error -= ResolverListener_Error;
                resolverListener.Dispose();
                resolverListener = null;
                return true;
            }
            else
            {
                Debug.WriteLine("Warning: ResolveListener already Disposed!");
                return false;
            }
        }

        /// <summary>
        /// Called by the ZeroConf Library when an error has occurred.
        /// </summary>
        /// <param name="sender"> </param>
        /// <param name="e"> </param>
        private void ResolverListener_Error(object sender, Exception exp)
        {
            Debug.WriteLine($"Error Occurred: {exp.Message}, {exp.InnerException.Message}");
            ErrorOccured(exp);
        }

        /// <summary>
        /// Called by the ZeroConf Library when a new device (or service) has been found
        /// </summary>
        /// <param name="sender"> </param>
        /// <param name="e"> </param>
        private void ResolverListener_ServiceFound(object sender, IZeroconfHost host)
        {
            Debug.WriteLine($"New Service Found: {host}");
            string output = AssemblePayload(host);
            DeviceDiscovered(output);
        }

        /// <summary>
        /// Called by the ZeroConf Library when a new device (or service) has been found
        /// </summary>
        /// <param name="sender"> </param>
        /// <param name="e"> </param>
        private void ResolverListener_ServiceLost(object sender, IZeroconfHost host)
        {
            Debug.WriteLine($"New Service Found: {host}");
            string output = AssemblePayload(host);
            DeviceLost(output);
        }

        /// <summary>
        /// Assembles the Service Payload (as expected by the ACASIA app) and return it as a JSON string.
        /// </summary>
        /// <param name="host">The information about the device</param>
        /// <returns>The Service Payload as a JSON string</returns>
        private string AssemblePayload(IZeroconfHost host)
        {
            // First, find the correct service message, its key must be the same as our protocol.
            IService service = host.Services.FirstOrDefault(x => string.Compare(x.Key, PROTOCOL) == 0).Value;
            
            //Fill the discovered service payload 
            ServicePayLoad payLoad = new ServicePayLoad
            {
                Host = host.DisplayName, 
                FullName = host.DisplayName + "." + service.Name ?? string.Empty,
                Name = service.Properties.First().FirstOrDefault(x => string.Compare(x.Key, "Identity") == 0).Value ?? string.Empty, 
                Port = (service != null) ? service.Port : 0
            };

            // Copy each IP adress to payload.
            foreach (string ip in host.IPAddresses)
            {
                payLoad.IPAddresses.Add(ip);
            }

            // Convert the payload to JSON.
            return JsonConvert.SerializeObject(payLoad);
        }
    }
}