// RenderingEngine.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "vulkan\vulkan.h"
#include <iostream>

#define ASSERT_VK(val)\
	if(val != VK_SUCCESS){\
		__debugbreak();\
	}

VkInstance instance;

void printStats(VkPhysicalDevice &dev) {
	VkPhysicalDeviceProperties devProperties;
	vkGetPhysicalDeviceProperties(dev, &devProperties);
	std::cout << "Name: " << devProperties.deviceName << std::endl << "API Version: " << VK_VERSION_MAJOR(devProperties.apiVersion) <<"." 
		<< VK_VERSION_MINOR(devProperties.apiVersion) << "." << VK_VERSION_PATCH(devProperties.apiVersion) <<std::endl 
		<< "Driver Version: " << devProperties.driverVersion << std::endl  
		<< "Vendor ID:" << devProperties.vendorID << std::endl
		<< "Device Type: " << devProperties.deviceType << std::endl
		<<std::endl;

	VkPhysicalDeviceFeatures devFeatures;
	vkGetPhysicalDeviceFeatures(dev, &devFeatures);

	std::cout << std::endl;
}

int main() {

	VkApplicationInfo appInfo;
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = NULL;
	appInfo.pApplicationName = "RenderingEngine";
	appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 0);
	appInfo.pEngineName = "RenderingEngine";
	appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_1;

	VkInstanceCreateInfo instanceInfo;
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pNext = NULL;
	instanceInfo.flags = 0;
	instanceInfo.pApplicationInfo = &appInfo;
	instanceInfo.enabledLayerCount = 0;
	instanceInfo.ppEnabledLayerNames = NULL;
	instanceInfo.enabledExtensionCount = 0;
	instanceInfo.ppEnabledExtensionNames = NULL;

	VkInstance instance;
	ASSERT_VK(vkCreateInstance(&instanceInfo, NULL, &instance));
	

	uint32_t numberPhysDevices = 0;
	ASSERT_VK(vkEnumeratePhysicalDevices(instance, &numberPhysDevices, NULL));

	VkPhysicalDevice* physDevices = new VkPhysicalDevice[numberPhysDevices];

	ASSERT_VK(vkEnumeratePhysicalDevices(instance, &numberPhysDevices, physDevices));

	for (int i = 0; i < numberPhysDevices; i++) {
		printStats(physDevices[i]);
	}



	return 0;
}

