// RenderingEngine.cpp : Defines the entry point for the console application.
//


#include "stdafx.h"

#define VULKAN_VERSION VK_API_VERSION_1_1

#include "Utils.h"
#include "ImageHolder.h"
#include "DepthImage.h"
#include "Vertex.h"
#include "MeshHolder.h"
#include "Resources.h"
#include "Settings.h"


#include <chrono>

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <assert.h>


//Do not delete for now
uint32_t						screenWidth = 1600;
uint32_t						screenHeight = 900;
uint32_t						amountImagesInSwapchain;

std::vector<VkSemaphore>		available;
std::vector<VkSemaphore>		finished;
std::vector<VkFence>			inFlightFences;

//Textures
const VkFormat					usedFormat = VK_FORMAT_B8G8R8A8_UNORM;

//Meshes
VkBuffer						vertexBuffer;//take a look at create buffer
VkDeviceMemory					vertexBufferDeviceMemory;
VkBuffer						indexBuffer;
VkDeviceMemory					indexBufferDeviceMemory;
std::vector<Vertex>				vertices = {};
std::vector<uint32_t>			indices = {};

//Materials
VkShaderModule					shaderModuleVert;
VkShaderModule					shaderModuleFrag;

std::vector<VkBuffer>			uniformBuffers;
std::vector<VkDeviceMemory>		uniformBuffersMemory;

VkRenderPass					renderPass;

//Scene
MeshHolder*						dragonMesh;
ImageHolder*					debugTexture;

//Other
auto							gameStartTime = std::chrono::high_resolution_clock::now();

//VkDebugReportCallbackEXT		callback;
VkDebugUtilsMessengerEXT		debugMessenger;
std::vector<VkImageView>		imageViews;
VkPipelineLayout				pipelineLayout;
VkPipeline						pipeline;
std::vector<VkCommandBuffer>	commandBuffers;
VkDescriptorSetLayout			descriptorSetLayout;
std::vector<VkDescriptorSet>	descriptorSets;

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
	glm::vec3 lighPos;
};
UniformBufferObject				ubo;


static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT messageType,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData) {

	std::cerr << "Validation Layers: " << pCallbackData->pMessage << std::endl << std::endl;

	return VK_FALSE;
}


void printStats(const VkPhysicalDevice& device) {
	
	VkPhysicalDeviceProperties devProperties;
	vkGetPhysicalDeviceProperties(device, &devProperties);
	
	std::cout << "Name: " << devProperties.deviceName << std::endl << "API Version: " << VK_VERSION_MAJOR(devProperties.apiVersion) << "."
		<< VK_VERSION_MINOR(devProperties.apiVersion) << "." << VK_VERSION_PATCH(devProperties.apiVersion) << std::endl
		<< "Driver Version: " << devProperties.driverVersion << std::endl
		<< "Vendor ID:" << devProperties.vendorID << std::endl
		<< "Device Type: " << devProperties.deviceType << std::endl
		<< "discreteQueuePriorities: " << devProperties.limits.discreteQueuePriorities
		<< std::endl;

	VkPhysicalDeviceFeatures devFeatures;
	vkGetPhysicalDeviceFeatures(device, &devFeatures);

	VkPhysicalDeviceMemoryProperties memProps;
	vkGetPhysicalDeviceMemoryProperties(device, &memProps);

	uint32_t amountQueueFamilies;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &amountQueueFamilies, nullptr);

	VkQueueFamilyProperties* queueFamilyProperties = new VkQueueFamilyProperties[amountQueueFamilies];
	vkGetPhysicalDeviceQueueFamilyProperties(device, &amountQueueFamilies, queueFamilyProperties);

	std::cout << "Amount of Queuefamilies: " << amountQueueFamilies << std::endl;

	for (unsigned int i = 0; i < amountQueueFamilies; i++) {
		
		uint32_t width = queueFamilyProperties[i].minImageTransferGranularity.width;
		uint32_t height = queueFamilyProperties[i].minImageTransferGranularity.height;
		uint32_t depth = queueFamilyProperties[i].minImageTransferGranularity.depth;
		
		std::cout << std::endl << "Queuefamily Nr: " << i << " VK_QUEUE_GRAPHICS_BIT       "
			<< ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
			<< std::endl << "Queuefamily Nr: " << i <<       " VK_QUEUE_COMPUTE_BIT        "
			<< ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0)
			<< std::endl << "Queuefamily Nr: " << i <<       " VK_QUEUE_TRANSFER_BIT       "
			<< ((queueFamilyProperties[i].queueFlags & VK_QUEUE_TRANSFER_BIT) != 0)
			<< std::endl << "Queuefamily Nr: " << i <<       " VK_QUEUE_SPARSE_BINDING_BIT "
			<< ((queueFamilyProperties[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT) != 0) 
			<< std::endl << "Queuecount: " << queueFamilyProperties[i].queueCount
			<< std::endl << "Timestamp Valid Bits: " << queueFamilyProperties[i].timestampValidBits 
			<< std::endl << "Min Image Timestamp Granularity " << width << ", " << height << ", " << depth
			<< std::endl << std::endl;
	}

	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, Settings::getSurface(), &surfaceCapabilities);

	std::cout << "SurfaceCapabilities" << std::endl;
	std::cout << "\tminImageCount: " << surfaceCapabilities.minImageCount << std::endl;
	std::cout << "\tmaxImageCount: " << surfaceCapabilities.maxImageCount << std::endl;
	std::cout << "\tcurrentExtent: " << surfaceCapabilities.currentExtent.width << ":" << surfaceCapabilities.currentExtent.height << std::endl;
	std::cout << "\tminImageExtent: " << surfaceCapabilities.minImageExtent.width << ":" << surfaceCapabilities.minImageExtent.height << std::endl;
	std::cout << "\tmaxImageExtent: " << surfaceCapabilities.maxImageExtent.width << ":" << surfaceCapabilities.maxImageExtent.height << std::endl;
	std::cout << "\tmaxImageArrayLayers: " << surfaceCapabilities.maxImageArrayLayers << std::endl;
	std::cout << "\tsupportedTransforms:" << surfaceCapabilities.supportedTransforms << std::endl;
	std::cout << "\tcurrentTransform:" << surfaceCapabilities.currentTransform << std::endl;
	std::cout << "\tsupportedCompositeAlpha:" << surfaceCapabilities.supportedCompositeAlpha << std::endl;
	std::cout << "\tsupportedUsageFlags:" << surfaceCapabilities.supportedUsageFlags << std::endl;


	uint32_t amountFormats = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, Settings::getSurface(), &amountFormats, nullptr);

	VkSurfaceFormatKHR *surfaceFormats = new VkSurfaceFormatKHR[amountFormats];
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, Settings::getSurface(), &amountFormats, surfaceFormats);

	std::cout << std::endl << "Amount of Formats: " << amountFormats << std::endl;
	for (uint32_t i = 0; i < amountFormats; i++) {
		
		std::cout << "Format: " << surfaceFormats[i].format << std::endl;
	}

	uint32_t amountPresentationmodes = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, Settings::getSurface(), &amountPresentationmodes, nullptr);

	VkPresentModeKHR *presentationsmodes = new VkPresentModeKHR[amountPresentationmodes];
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, Settings::getSurface(), &amountPresentationmodes, presentationsmodes);

	std::cout << std::endl << "Amount of Presentmodes: " << amountPresentationmodes << std::endl;
	for (uint32_t i = 0; i < amountPresentationmodes; i++) {
		
		std::cout << "Presentmode: " << presentationsmodes[i] << std::endl;
	}

	std::cout << std::endl;
	delete[] surfaceFormats;
	delete[] queueFamilyProperties;
}

void recreateSwapchain();
void onWindowResized(GLFWwindow* window, int width, int height) {

	while (width == 0 || height == 0) {

		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
	}
	Settings::getScreenWidth() = width;
	Settings::getScreenHeight() = height;
	screenWidth = width;
	screenHeight = height;

	recreateSwapchain();
}

void startGLFW() {

	glfwInit(); 
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	//TODO Investigate why not Settings::getScreenWidth()
	Settings::getWindow() = glfwCreateWindow(screenWidth, screenHeight, "Tech Demo", nullptr, nullptr);
	glfwSetWindowSizeCallback(Settings::getWindow(), onWindowResized);
}


void checkExtensionSupport() {//TODO find Solution to iteration over VkExtensionProperties


	uint32_t amountSupportedExtensions = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &amountSupportedExtensions, nullptr);

	std::vector<VkExtensionProperties> supportedExtensionProperties(amountSupportedExtensions);
	vkEnumerateInstanceExtensionProperties(nullptr, &amountSupportedExtensions, supportedExtensionProperties.data());

	for (const char* extension : Settings::getInstanceExtensions()) {

		bool found = false;
		for (const VkExtensionProperties extensionProperties : supportedExtensionProperties) {

			if (!strcmp(extension, extensionProperties.extensionName)) {
				
				found = true;
				break;
			}
		}
		if (!found) {

			notSupported("An extension was not supported!");
		}
	}

}

void checkValidationLayerSupport() {

	uint32_t amountSupportedValidationLayers = 0;
	vkEnumerateInstanceLayerProperties(&amountSupportedValidationLayers, nullptr);

	std::vector<VkLayerProperties> supportedValidationLayerProperties(amountSupportedValidationLayers);
	vkEnumerateInstanceLayerProperties(&amountSupportedValidationLayers, supportedValidationLayerProperties.data());


	auto layers = Settings::getValidationLayers();
	for (const char* layer : layers) {

		bool found = false;
		for (const VkLayerProperties layerProperties : supportedValidationLayerProperties) {

			if (!strcmp(layer, layerProperties.layerName)) {
				
				found = true;
				break;
			}
		}
		if (!found) {
			
			notSupported("An extension was not supported!");
		}
	}

}

void createInstance() {
	
	VkApplicationInfo appInfo;
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = nullptr;
	appInfo.pApplicationName = "RenderingEngine";
	appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 0);
	appInfo.pEngineName = "RenderingEngine";
	appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 0);
	appInfo.apiVersion = VULKAN_VERSION;

	uint32_t amountGLFWExtensions = 0;
	const char** extensions = glfwGetRequiredInstanceExtensions(&amountGLFWExtensions);
	Settings::getInstanceExtensions().insert(Settings::getInstanceExtensions().end(), extensions, &extensions[amountGLFWExtensions]);
	
	checkExtensionSupport();
	checkValidationLayerSupport();

	VkInstanceCreateInfo instanceInfo;
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pNext = nullptr;
	instanceInfo.flags = 0;
	instanceInfo.pApplicationInfo = &appInfo;
#ifdef NDEBUG
	instanceInfo.enabledLayerCount = 0;
	instanceInfo.ppEnabledLayerNames = nullptr;
#else 
	instanceInfo.enabledLayerCount = Settings::getValidationLayers().size();
	instanceInfo.ppEnabledLayerNames = Settings::getValidationLayers().data();
#endif
	
	instanceInfo.enabledExtensionCount = Settings::getInstanceExtensions().size();
	instanceInfo.ppEnabledExtensionNames = Settings::getInstanceExtensions().data();
	std::cout << &instanceInfo << " " << &Settings::getInstance();

	ASSERT_VK(vkCreateInstance(&instanceInfo, nullptr, &Settings::getInstance()));
}

void printValidationLayers() {
	
	uint32_t layerAmount = 0;
	vkEnumerateInstanceLayerProperties(&layerAmount, nullptr);

	std::vector<VkLayerProperties> layers(layerAmount);
	vkEnumerateInstanceLayerProperties(&layerAmount, layers.data());

	std::cout << "Amount of layers: " << layerAmount << std::endl;
	for (uint32_t i = 0; i < layerAmount; i++) {
		
		std::cout << "Name:        " << layers.at(i).layerName << std::endl
			<< "Description: " << layers.at(i).description << std::endl << std::endl;
	}

}

void printExtensions() {
	
	uint32_t extensionAmount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionAmount, nullptr);

	std::vector<VkExtensionProperties> extensions(extensionAmount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionAmount, extensions.data());

	std::cout << std::endl << "Amount of Extension: " << extensionAmount << std::endl;
	for (uint32_t i = 0; i < extensionAmount; i++) {
		
		std::cout << "Name:        " << extensions.at(i).extensionName << std::endl;
	}

}

VkResult createDebugUtilsMessengerEXT(const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(Settings::getInstance(), "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		
		return func(Settings::getInstance(), pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void setUpDebugCallbacks() {

	VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
	createInfo.pUserData = nullptr; // Optional

	ASSERT_VK(createDebugUtilsMessengerEXT(&createInfo, nullptr, &debugMessenger));

}

void destroyDebugUtilsMessengerEXT(VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
	
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(Settings::getInstance(), "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(Settings::getInstance(), debugMessenger, pAllocator);
	}
}

bool deviceSuitable(VkPhysicalDevice device) {
	//TODO elaborate the picking process
	//e.g. queue families and shader capabilities

	uint32_t amountSupportedDeviceExtensions = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &amountSupportedDeviceExtensions, nullptr);

	std::vector<VkExtensionProperties>supportedExtensionProperties(amountSupportedDeviceExtensions);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &amountSupportedDeviceExtensions, supportedExtensionProperties.data());

	for (const char* extension : Settings::getDeviceExtensions()) {

		bool found = false;
		for (const VkExtensionProperties extensionProperties : supportedExtensionProperties) {

			if (!strcmp(extension, extensionProperties.extensionName)) {
				
				found = true;
				break;
			}
		}
		if (!found) {
			
			notSupported("An extension was not supported!");
		}
	}

	return true;
}

void pickActivePhysicalDevices() {

	uint32_t numberPhysDevices = 0;
	ASSERT_VK(vkEnumeratePhysicalDevices(Settings::getInstance(), &numberPhysDevices, nullptr));

	if (!numberPhysDevices) notSupported("No GPU with Vulkan support found!");
	std::vector<VkPhysicalDevice> physicalDevices(numberPhysDevices);
	ASSERT_VK(vkEnumeratePhysicalDevices(Settings::getInstance(), &numberPhysDevices, physicalDevices.data()));

	bool found = false;
	for (uint8_t i = 0; i < physicalDevices.size(); i++) {
		
		if (deviceSuitable(physicalDevices.at(i))) {
			
			Settings::getPhysDevice() = physicalDevices.at(i);
			found = true;
			break;
		}
	}
	if (!found) notSupported("Could not find a GPU that meets the engine requirements!");

	for (uint32_t i = 0; i < numberPhysDevices; i++) {
		
		//printStats(physicalDevices[i]);
	}
}

void findQueueFamilyIndex(VkQueueFlags queueFlags, uint32_t &writeIndex) {
	
	//TODO: Improve selection of queue family indices
	uint32_t amountQueueFamilies = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(Settings::getPhysDevice(), &amountQueueFamilies, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilyProperties(amountQueueFamilies);
	vkGetPhysicalDeviceQueueFamilyProperties(Settings::getPhysDevice(), &amountQueueFamilies, queueFamilyProperties.data());

	for (uint32_t i = 0; i < amountQueueFamilies; i++) {
		
		if ((queueFamilyProperties[i].queueFlags & queueFlags) == queueFlags) {
			
			writeIndex = i;
			return;
		}
	}

	notSupported("Could not find suitable Queuefamily!");

}

void createDevice() {

	uint32_t amountFamilies = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(Settings::getPhysDevice(), &amountFamilies, nullptr);
	
	findQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, Settings::getGraphicsFamilyIndex());
	findQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT, Settings::getTransferFamilyIndex());
	findQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT, Settings::getPresentFamilyIndex());

	std::vector<float> queuePriorities(amountFamilies);
	std::fill(queuePriorities.begin(), queuePriorities.end(), 1.0f);

	std::vector<VkDeviceQueueCreateInfo> devQueueCreateInfos(1);
	devQueueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	devQueueCreateInfos[0].pNext = nullptr;
	devQueueCreateInfos[0].flags = 0;
	devQueueCreateInfos[0].queueFamilyIndex = Settings::getGraphicsFamilyIndex();
	devQueueCreateInfos[0].queueCount = 2;
	devQueueCreateInfos[0].pQueuePriorities = queuePriorities.data();

	//TODO Fix queues
	/*devQueueCreateInfos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	devQueueCreateInfos[1].pNext = nullptr;
	devQueueCreateInfos[1].flags = 0;
	devQueueCreateInfos[1].queueFamilyIndex = Settings::getTransferFamilyIndex();
	devQueueCreateInfos[1].queueCount = 1;
	devQueueCreateInfos[1].pQueuePriorities = queuePriorities.data();*/

	VkPhysicalDeviceFeatures enabledFeatures = {};
	enabledFeatures.samplerAnisotropy = VK_TRUE;
	
	VkDeviceCreateInfo devCreateInfo;
	devCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	devCreateInfo.pNext = nullptr;
	devCreateInfo.flags = 0;
	devCreateInfo.queueCreateInfoCount = devQueueCreateInfos.size();
	devCreateInfo.pQueueCreateInfos = devQueueCreateInfos.data();
	devCreateInfo.enabledLayerCount = 0;
	devCreateInfo.ppEnabledLayerNames = nullptr;
	devCreateInfo.enabledExtensionCount = Settings::getDeviceExtensions().size();
	devCreateInfo.ppEnabledExtensionNames = Settings::getDeviceExtensions().data();
	devCreateInfo.pEnabledFeatures = &enabledFeatures;

	//TODO: Pick best suitable device instead of first one
	ASSERT_VK(vkCreateDevice(Settings::getPhysDevice(), &devCreateInfo, nullptr, &Settings::getDevice()));

}

void getQueues() {

	vkGetDeviceQueue(Settings::getDevice(), Settings::getGraphicsFamilyIndex(), 0, &Settings::getGraphicsQueue());
	vkGetDeviceQueue(Settings::getDevice(), Settings::getPresentFamilyIndex(), 0, &Settings::getPresentQueue());
	vkGetDeviceQueue(Settings::getDevice(), Settings::getTransferFamilyIndex(), 0, &Settings::getTransferQueue());
}

void checkSurfaceSupport() {
	
	VkBool32 surfaceSupport = false;
	ASSERT_VK(vkGetPhysicalDeviceSurfaceSupportKHR(Settings::getPhysDevice(), 0, Settings::getSurface(), &surfaceSupport));

	if (!surfaceSupport) {
		
		std::cerr << "Surface not supported!" << std::endl;
	}
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat() {
	
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(Settings::getPhysDevice(), Settings::getSurface(), &formatCount, nullptr);
	
	std::vector<VkSurfaceFormatKHR> availableFormats(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(Settings::getPhysDevice(), Settings::getSurface(), &formatCount, availableFormats.data());

	if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
		
		return VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	for (const auto& availableFormat : availableFormats) {
		
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
			
			return availableFormat;
		}
	}

	return availableFormats.at(0);
}

VkPresentModeKHR chooseSwapPresentMode() {
	
	uint32_t modeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(Settings::getPhysDevice(), Settings::getSurface(), &modeCount, nullptr);

	std::vector<VkPresentModeKHR> availablePresentModes(modeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(Settings::getPhysDevice(), Settings::getSurface(), &modeCount, availablePresentModes.data());
	
	for (const auto& mode : availablePresentModes) {
		
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
	}
	
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent() {
	
	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Settings::getPhysDevice(), Settings::getSurface(), &capabilities);

	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		
		return capabilities.currentExtent;
	}
	else {
		
		int width, height;
		glfwGetFramebufferSize(Settings::getWindow(), &width, &height);
		VkExtent2D actualExtent = { Settings::getScreenWidth(), Settings::getScreenHeight() };

		actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}

void chooseSwapchainImageCount() {

	VkSurfaceCapabilitiesKHR capabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Settings::getPhysDevice(), Settings::getSurface(), &capabilities);

	amountImagesInSwapchain = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0 && amountImagesInSwapchain > capabilities.maxImageCount) {
		
		amountImagesInSwapchain = capabilities.maxImageCount;
	}
}

void createSwapchain() {

	chooseSwapchainImageCount();

	Settings::getSurfaceFormat() = chooseSwapSurfaceFormat();
	Settings::getPresentMode() = chooseSwapPresentMode();
	Settings::getSwapchainExtent() = chooseSwapExtent();

	VkSwapchainCreateInfoKHR swapchainCreateInfo;
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.pNext = nullptr;
	swapchainCreateInfo.flags = 0;
	swapchainCreateInfo.surface = Settings::getSurface();

	swapchainCreateInfo.minImageCount = amountImagesInSwapchain;
	swapchainCreateInfo.imageFormat = Settings::getSurfaceFormat().format;
	swapchainCreateInfo.imageColorSpace = Settings::getSurfaceFormat().colorSpace;
	swapchainCreateInfo.imageExtent = Settings::getSwapchainExtent();
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.queueFamilyIndexCount = 0;
	swapchainCreateInfo.pQueueFamilyIndices = nullptr;
	swapchainCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode = Settings::getPresentMode();
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.oldSwapchain = Settings::getSwapchain();

	ASSERT_VK(vkCreateSwapchainKHR(Settings::getDevice(), &swapchainCreateInfo, nullptr, &Settings::getSwapchain()));
}

void createImageViews() {
	
	vkGetSwapchainImagesKHR(Settings::getDevice(), Settings::getSwapchain(), &amountImagesInSwapchain, nullptr);
	std::vector<VkImage> swapchainImages(amountImagesInSwapchain);
	ASSERT_VK(vkGetSwapchainImagesKHR(Settings::getDevice(), Settings::getSwapchain(), &amountImagesInSwapchain, swapchainImages.data()));

	imageViews.resize(amountImagesInSwapchain);
	for (uint32_t i = 0; i < amountImagesInSwapchain; i++) {

		imageViews.at(i) = createImageView( swapchainImages.at(i), Settings::getSurfaceFormat().format, VK_IMAGE_ASPECT_COLOR_BIT);
	}

}

void createRenderPass() {
	
	VkAttachmentDescription attachmentDescription;
	attachmentDescription.flags = 0;
	attachmentDescription.format = Settings::getSurfaceFormat().format;
	attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference attachmentReference;
	attachmentReference.attachment = 0;
	attachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentDescription depthAttachmentDescription = DepthImage::getDepthAttachmentDescription();

	VkAttachmentReference depthAttachmentReference;
	depthAttachmentReference.attachment = 1;
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


	VkSubpassDescription subpassDescription;
	subpassDescription.flags = 0;
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.inputAttachmentCount = 0;
	subpassDescription.pInputAttachments = nullptr;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &attachmentReference;
	subpassDescription.pResolveAttachments = nullptr;
	subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;
	subpassDescription.preserveAttachmentCount = 0;
	subpassDescription.pPreserveAttachments = nullptr;

	VkSubpassDependency subpassDependency;
	subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependency.dstSubpass = 0;
	subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependency.srcAccessMask = 0;
	subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependency.dependencyFlags = 0;

	std::vector<VkAttachmentDescription> attachmentDescriptions;
	attachmentDescriptions.push_back(attachmentDescription);
	attachmentDescriptions.push_back(depthAttachmentDescription);

	VkRenderPassCreateInfo renderPassCreateInfo;
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.pNext = nullptr;
	renderPassCreateInfo.flags = 0;
	renderPassCreateInfo.attachmentCount = attachmentDescriptions.size();
	renderPassCreateInfo.pAttachments = attachmentDescriptions.data();
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpassDescription;
	renderPassCreateInfo.dependencyCount = 1;
	renderPassCreateInfo.pDependencies = &subpassDependency;

	ASSERT_VK(vkCreateRenderPass(Settings::getDevice(), &renderPassCreateInfo, nullptr, &renderPass));
}

void createDescriptorSetLayout() {
	
	VkDescriptorSetLayoutBinding descriptorSetLayoutBinding;
	descriptorSetLayoutBinding.binding = 0;
	descriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorSetLayoutBinding.descriptorCount = 1;
	descriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	descriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding samplerDescriptorSetLayoutBinding;
	samplerDescriptorSetLayoutBinding.binding = 1;
	samplerDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerDescriptorSetLayoutBinding.descriptorCount = 1;
	samplerDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	std::vector<VkDescriptorSetLayoutBinding> descriptorSets;
	descriptorSets.push_back(descriptorSetLayoutBinding);
	descriptorSets.push_back(samplerDescriptorSetLayoutBinding);

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = nullptr;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = descriptorSets.size();
	descriptorSetLayoutCreateInfo.pBindings = descriptorSets.data();

	ASSERT_VK(vkCreateDescriptorSetLayout(Settings::getDevice(), &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout));

}

void createPipeline() {

	Resources::loadAllShaders();
	shaderModuleVert = Resources::getShader("vert.spv");
	shaderModuleFrag = Resources::getShader("frag.spv");

	VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfoVert;
	pipelineShaderStageCreateInfoVert.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineShaderStageCreateInfoVert.pNext = nullptr;
	pipelineShaderStageCreateInfoVert.flags = 0;
	pipelineShaderStageCreateInfoVert.stage = VK_SHADER_STAGE_VERTEX_BIT;
	pipelineShaderStageCreateInfoVert.module = shaderModuleVert;
	pipelineShaderStageCreateInfoVert.pName = "main";
	pipelineShaderStageCreateInfoVert.pSpecializationInfo = nullptr;

	VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfoFrag;
	pipelineShaderStageCreateInfoFrag.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	pipelineShaderStageCreateInfoFrag.pNext = nullptr;
	pipelineShaderStageCreateInfoFrag.flags = 0;
	pipelineShaderStageCreateInfoFrag.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	pipelineShaderStageCreateInfoFrag.module = shaderModuleFrag;
	pipelineShaderStageCreateInfoFrag.pName = "main";
	pipelineShaderStageCreateInfoFrag.pSpecializationInfo = nullptr;

	VkPipelineShaderStageCreateInfo shaderStages[] = { pipelineShaderStageCreateInfoVert, pipelineShaderStageCreateInfoFrag };

	VkVertexInputBindingDescription vertexInputBindingDescription = Vertex::getVertexInputBindingDescription();
	std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescritions = Vertex::getVertexInputAttributDescriptions();

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo;
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = 0;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexInputBindingDescription;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = vertexInputAttributeDescritions.size();
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInputAttributeDescritions.data();

	VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo;
	pipelineInputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	pipelineInputAssemblyStateCreateInfo.pNext = nullptr;
	pipelineInputAssemblyStateCreateInfo.flags = 0;
	pipelineInputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkViewport viewPort;
	viewPort.x = 0.0f;
	viewPort.y = 0.0f;
	viewPort.width = static_cast<float>(Settings::getSwapchainExtent().width);
	viewPort.height = static_cast<float>(Settings::getSwapchainExtent().height);
	viewPort.minDepth = 0.0f;
	viewPort.maxDepth = 1.0f;

	VkRect2D scissor;
	scissor.offset = { 0, 0 };
	scissor.extent = Settings::getSwapchainExtent();

	VkPipelineViewportStateCreateInfo pipelineViewportStateCreateInfo;
	pipelineViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	pipelineViewportStateCreateInfo.pNext = nullptr;
	pipelineViewportStateCreateInfo.flags = 0;
	pipelineViewportStateCreateInfo.viewportCount = 1;
	pipelineViewportStateCreateInfo.pViewports = &viewPort;
	pipelineViewportStateCreateInfo.scissorCount = 1;
	pipelineViewportStateCreateInfo.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo;
	pipelineRasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	pipelineRasterizationStateCreateInfo.pNext = nullptr;
	pipelineRasterizationStateCreateInfo.flags = 0;
	pipelineRasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	pipelineRasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	pipelineRasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	pipelineRasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	pipelineRasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	pipelineRasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	pipelineRasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	pipelineRasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	pipelineRasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
	pipelineRasterizationStateCreateInfo.lineWidth = 1.0f;
	
	VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo;
	pipelineMultisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	pipelineMultisampleStateCreateInfo.pNext = nullptr;
	pipelineMultisampleStateCreateInfo.flags = 0;
	pipelineMultisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	pipelineMultisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	pipelineMultisampleStateCreateInfo.minSampleShading = 1.0f;
	pipelineMultisampleStateCreateInfo.pSampleMask = nullptr;
	pipelineMultisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	pipelineMultisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfoOpague = DepthImage::getPipelineDepthStencilStateCreateInfoOpaque();

	VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState;
	pipelineColorBlendAttachmentState.blendEnable = VK_TRUE;
	pipelineColorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	pipelineColorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	pipelineColorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	pipelineColorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	pipelineColorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	pipelineColorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	pipelineColorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo;
	pipelineColorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	pipelineColorBlendStateCreateInfo.pNext = nullptr;
	pipelineColorBlendStateCreateInfo.flags = 0;
	pipelineColorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	pipelineColorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_NO_OP;
	pipelineColorBlendStateCreateInfo.attachmentCount = 1;
	pipelineColorBlendStateCreateInfo.pAttachments = &pipelineColorBlendAttachmentState;
	pipelineColorBlendStateCreateInfo.blendConstants[0] = 0.0f;
	pipelineColorBlendStateCreateInfo.blendConstants[1] = 0.0f;
	pipelineColorBlendStateCreateInfo.blendConstants[2] = 0.0f;
	pipelineColorBlendStateCreateInfo.blendConstants[3] = 0.0f;

	VkDynamicState dynamicStates[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo;
	pipelineDynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	pipelineDynamicStateCreateInfo.pNext = nullptr;
	pipelineDynamicStateCreateInfo.flags = 0;
	pipelineDynamicStateCreateInfo.dynamicStateCount = sizeof(dynamicStates)/sizeof(*dynamicStates);
	pipelineDynamicStateCreateInfo.pDynamicStates = dynamicStates;

	VkPushConstantRange pushConstantRange;
	pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(glm::vec3);

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

	ASSERT_VK(vkCreatePipelineLayout(Settings::getDevice(), &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

	VkGraphicsPipelineCreateInfo pipelineCreateInfo;
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.pNext = nullptr;
	pipelineCreateInfo.flags = 0;
	pipelineCreateInfo.stageCount = 2;
	pipelineCreateInfo.pStages = shaderStages;
	pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	pipelineCreateInfo.pInputAssemblyState = &pipelineInputAssemblyStateCreateInfo;
	pipelineCreateInfo.pTessellationState = nullptr;
	pipelineCreateInfo.pViewportState = &pipelineViewportStateCreateInfo;
	pipelineCreateInfo.pRasterizationState = &pipelineRasterizationStateCreateInfo;
	pipelineCreateInfo.pMultisampleState = &pipelineMultisampleStateCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &pipelineDepthStencilStateCreateInfoOpague;
	pipelineCreateInfo.pColorBlendState = &pipelineColorBlendStateCreateInfo;
	pipelineCreateInfo.pDynamicState = &pipelineDynamicStateCreateInfo;
	pipelineCreateInfo.layout = pipelineLayout;
	pipelineCreateInfo.renderPass = renderPass;
	pipelineCreateInfo.subpass = 0;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCreateInfo.basePipelineIndex = -1;
	 
	ASSERT_VK(vkCreateGraphicsPipelines(Settings::getDevice(), VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline));
}

void createFrameBuffers() {
	
	Settings::getFramebuffers().resize(amountImagesInSwapchain);
	for (size_t i = 0; i < amountImagesInSwapchain; i++) {

		std::vector<VkImageView> attachmentViews;
		attachmentViews.push_back(imageViews[i]);
		attachmentViews.push_back(Settings::getDepthImage()->imageView);

		VkFramebufferCreateInfo framebufferCreateInfo;
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.pNext = nullptr;
		framebufferCreateInfo.flags = 0;
		framebufferCreateInfo.renderPass = renderPass;
		framebufferCreateInfo.attachmentCount = attachmentViews.size();
		framebufferCreateInfo.pAttachments = attachmentViews.data();
		framebufferCreateInfo.width = Settings::getSwapchainExtent().width;
		framebufferCreateInfo.height = Settings::getSwapchainExtent().height;
		framebufferCreateInfo.layers = 1;

		ASSERT_VK(vkCreateFramebuffer(Settings::getDevice(), &framebufferCreateInfo, nullptr, &Settings::getFramebuffers().at(i)));
	}
}

void createDepthImage() {

	Settings::getDepthImage() = new DepthImage(Settings::getGraphicsQueue());
}

void createCommandPool() {
	
	VkCommandPoolCreateInfo commandPoolCreateInfo;
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.pNext = nullptr;
	commandPoolCreateInfo.flags = 0;
	commandPoolCreateInfo.queueFamilyIndex = Settings::getGraphicsFamilyIndex();

	ASSERT_VK(vkCreateCommandPool(Settings::getDevice(), &commandPoolCreateInfo, nullptr, &Settings::getCommandPool()));
}

void allocateCommandBuffers() {
	
	VkCommandBufferAllocateInfo commandBufferAllocateInfo;
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.pNext = nullptr;
	commandBufferAllocateInfo.commandPool = Settings::getCommandPool();
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = amountImagesInSwapchain;

	commandBuffers.resize(amountImagesInSwapchain);
	ASSERT_VK(vkAllocateCommandBuffers(Settings::getDevice(), &commandBufferAllocateInfo, commandBuffers.data()));
}

void loadTexture() {
	
	debugTexture = new ImageHolder("resources/grass.png");
	
	debugTexture->transferToGPU(Settings::getGraphicsQueue());
}

void loadMesh() {
	
	dragonMesh = new MeshHolder("resources/dragon.obj");
	vertices = dragonMesh->getVertices();
	indices = dragonMesh->getIndices();
}

void createVertexBuffer() {
	//TODO Push into Mesh
	createAndUploadBuffer<Vertex>( vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBuffer, vertexBufferDeviceMemory);
}

void createIndexBuffer() {
	
	createAndUploadBuffer<uint32_t>( indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBuffer, indexBufferDeviceMemory);
}

void createUniformBuffer() {

	VkDeviceSize deviceSize = sizeof(UniformBufferObject);
	uniformBuffers.resize(amountImagesInSwapchain);
	uniformBuffersMemory.resize(amountImagesInSwapchain);
	
	for (uint32_t i = 0; i < amountImagesInSwapchain; ++i) {
		
		createBuffer(deviceSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, uniformBuffers.at(i),
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffersMemory.at(i));
	}
}

void createDescriptorPool() {
	
	VkDescriptorPoolSize descriptorPoolSize;
	descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorPoolSize.descriptorCount = amountImagesInSwapchain;

	VkDescriptorPoolSize samplerDescriptorPoolSize;
	samplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerDescriptorPoolSize.descriptorCount = amountImagesInSwapchain;

	std::vector<VkDescriptorPoolSize> descriptorPoolSizes;
	descriptorPoolSizes.push_back(descriptorPoolSize);
	descriptorPoolSizes.push_back(samplerDescriptorPoolSize);

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = amountImagesInSwapchain;
	descriptorPoolCreateInfo.poolSizeCount = descriptorPoolSizes.size();
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();

	ASSERT_VK(vkCreateDescriptorPool(Settings::getDevice(), &descriptorPoolCreateInfo, nullptr, &Settings::getDescriptorPool()));
}

void allocateDescriptorSet() {
	
	std::vector<VkDescriptorSetLayout> layouts(amountImagesInSwapchain, descriptorSetLayout);

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.pNext = nullptr;
	descriptorSetAllocateInfo.descriptorPool = Settings::getDescriptorPool();
	descriptorSetAllocateInfo.descriptorSetCount = amountImagesInSwapchain;
	descriptorSetAllocateInfo.pSetLayouts = layouts.data();

	descriptorSets.resize(amountImagesInSwapchain);
	ASSERT_VK(vkAllocateDescriptorSets(Settings::getDevice(), &descriptorSetAllocateInfo, descriptorSets.data()));

	for (uint32_t i = 0; i < amountImagesInSwapchain; ++i) {
		
		VkDescriptorBufferInfo descriptorBufferInfo;
		descriptorBufferInfo.buffer = uniformBuffers[i];
		descriptorBufferInfo.offset = 0;
		descriptorBufferInfo.range = sizeof(UniformBufferObject);

		VkWriteDescriptorSet writeDescriptorSet;
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.pNext = nullptr;
		writeDescriptorSet.dstSet = descriptorSets[i];
		writeDescriptorSet.dstBinding = 0;
		writeDescriptorSet.dstArrayElement = 0;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeDescriptorSet.pImageInfo = nullptr;
		writeDescriptorSet.pBufferInfo = &descriptorBufferInfo;
		writeDescriptorSet.pTexelBufferView = nullptr;

		VkDescriptorImageInfo samplerDescriptorImageInfo;
		samplerDescriptorImageInfo.sampler = debugTexture->getSampler();
		samplerDescriptorImageInfo.imageView = debugTexture->getImageView();
		samplerDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet samplerWriteDescriptorSet;
		samplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		samplerWriteDescriptorSet.pNext = nullptr;
		samplerWriteDescriptorSet.dstSet = descriptorSets[i];
		samplerWriteDescriptorSet.dstBinding = 1;
		samplerWriteDescriptorSet.dstArrayElement = 0;
		samplerWriteDescriptorSet.descriptorCount = 1;
		samplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerWriteDescriptorSet.pImageInfo = &samplerDescriptorImageInfo;
		samplerWriteDescriptorSet.pBufferInfo = nullptr;
		samplerWriteDescriptorSet.pTexelBufferView = nullptr;

		std::vector<VkWriteDescriptorSet> writeDescriptorSets;
		writeDescriptorSets.push_back(writeDescriptorSet);
		writeDescriptorSets.push_back(samplerWriteDescriptorSet);

		vkUpdateDescriptorSets(Settings::getDevice(), writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);
	}
}

void recordCommands() {
	
	VkCommandBufferBeginInfo commandBufferBeginInfo;
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.pNext = nullptr;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	commandBufferBeginInfo.pInheritanceInfo = nullptr;

	for (size_t i = 0; i < amountImagesInSwapchain; i++) {
		
		ASSERT_VK(vkBeginCommandBuffer(commandBuffers[i], &commandBufferBeginInfo));

		VkClearValue clearValue = { 0.0f, 0.0f, 0.0f, 1.0f };
		VkClearValue depthClearValue = { 1.0f, 0.0f };
		std::vector<VkClearValue> clearValues;
		clearValues.push_back(clearValue);
		clearValues.push_back(depthClearValue);

		VkRenderPassBeginInfo renderPassBeginInfo;
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.pNext = nullptr;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = Settings::getFramebuffers().at(i);
		renderPassBeginInfo.renderArea.offset = { 0, 0 };
		renderPassBeginInfo.renderArea.extent = Settings::getSwapchainExtent();
		renderPassBeginInfo.clearValueCount = clearValues.size();
		renderPassBeginInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(commandBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		glm::vec3 col = { 1.0f, 0.0f, 0.0f };
		
		vkCmdPushConstants(commandBuffers[i], pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(glm::vec3), &col);

		vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		VkViewport viewPort;
		viewPort.x = 0.0f;
		viewPort.y = 0.0f;
		viewPort.width = static_cast<float>(Settings::getSwapchainExtent().width);
		viewPort.height = static_cast<float>(Settings::getSwapchainExtent().height);
		viewPort.minDepth = 0.0f;
		viewPort.maxDepth = 1.0f;

		vkCmdSetViewport(commandBuffers[i], 0, 1, &viewPort);

		VkRect2D scissor;
		scissor.offset = { 0, 0};
		scissor.extent = Settings::getSwapchainExtent();

		vkCmdSetScissor(commandBuffers[i], 0, 1, &scissor);

		VkDeviceSize offsets[] = { 0 };

		vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, &vertexBuffer, offsets);

		vkCmdBindIndexBuffer(commandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT32);
		
		vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[i], 0, nullptr);

		vkCmdDrawIndexed(commandBuffers[i], indices.size(), 1, 0, 0, 0);

		vkCmdEndRenderPass(commandBuffers[i]);

		ASSERT_VK(vkEndCommandBuffer(commandBuffers[i]));
	}
}

void createSemaphores() {

	available.resize(Settings::getMaxFramesInFlight());
	finished.resize(Settings::getMaxFramesInFlight());
	inFlightFences.resize(Settings::getMaxFramesInFlight());

	VkSemaphoreCreateInfo semaphoreCreateInfo;
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = nullptr;
	semaphoreCreateInfo.flags = 0;

	VkFenceCreateInfo fenceInfo;
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.pNext = nullptr;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	available.resize(Settings::getMaxFramesInFlight());
	finished.resize(Settings::getMaxFramesInFlight());
	for (size_t i = 0; i < Settings::getMaxFramesInFlight(); i++) {
		
		ASSERT_VK(vkCreateSemaphore(Settings::getDevice(), &semaphoreCreateInfo, nullptr, &available.at(i)/*&Settings::getAvailableSemaphores().at(i)*/));
		ASSERT_VK(vkCreateSemaphore(Settings::getDevice(), &semaphoreCreateInfo, nullptr, &finished.at(i)/*&Settings::getFinishedSemaphores().at(i)*/));

		ASSERT_VK(vkCreateFence(Settings::getDevice(), &fenceInfo, nullptr, &inFlightFences.at(i)));
	}
}

void recreateSwapchain() {

	vkDeviceWaitIdle(Settings::getDevice());


	vkFreeCommandBuffers(Settings::getDevice(), Settings::getCommandPool(), amountImagesInSwapchain, commandBuffers.data());
	
	for (size_t i = 0; i < amountImagesInSwapchain; i++) {
		
		vkDestroyFramebuffer(Settings::getDevice(), Settings::getFramebuffers().at(i), nullptr);
	}


	vkDestroyRenderPass(Settings::getDevice(), renderPass, nullptr);

	for (uint32_t i = 0; i < amountImagesInSwapchain; i++) {
		
		vkDestroyImageView(Settings::getDevice(), imageViews[i], nullptr);
	}

	VkSwapchainKHR oldSwapchain = Settings::getSwapchain();

	createSwapchain();
	createImageViews();
	createRenderPass();
	createDepthImage();
	createFrameBuffers();
	createCommandPool();
	allocateCommandBuffers();
	recordCommands();

	vkDestroySwapchainKHR(Settings::getDevice(), oldSwapchain, nullptr);
}

void startVulkan() {

	printValidationLayers();
	printExtensions();
	
	createInstance();
	setUpDebugCallbacks();
	ASSERT_VK(glfwCreateWindowSurface(Settings::getInstance(), Settings::getWindow(), nullptr, &Settings::getSurface()))
	pickActivePhysicalDevices();
	createDevice();
	getQueues();

	checkSurfaceSupport();
	createSwapchain();
	createImageViews();
	createRenderPass();
	createDescriptorSetLayout();

	createPipeline();
	createCommandPool();
	createDepthImage();
	createFrameBuffers();
	allocateCommandBuffers();

	loadTexture();
	loadMesh();

	createVertexBuffer();
	createIndexBuffer();
	createUniformBuffer();

	createDescriptorPool();
	allocateDescriptorSet();

	recordCommands();
	createSemaphores();
}

void updateMVP();

void drawFrame() {
	
	vkWaitForFences(Settings::getDevice(), 1, &inFlightFences.at(Settings::currentFrame()), VK_TRUE, std::numeric_limits<uint64_t>::max());
	vkResetFences(Settings::getDevice(), 1, &inFlightFences.at(Settings::currentFrame()));

	uint32_t imageIndex;
	vkAcquireNextImageKHR(Settings::getDevice(), Settings::getSwapchain(), std::numeric_limits<uint64_t>::max(), available.at(Settings::currentFrame()), VK_NULL_HANDLE, &imageIndex);

	VkPipelineStageFlags waitStageMasks[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	updateMVP();

	VkSubmitInfo submitInfo;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &available.at(Settings::currentFrame());
	submitInfo.pWaitDstStageMask = waitStageMasks;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &finished.at(Settings::currentFrame());

	vkResetFences(Settings::getDevice(), 1, &inFlightFences.at(Settings::currentFrame()));

	ASSERT_VK(vkQueueSubmit(Settings::getGraphicsQueue(), 1, &submitInfo, inFlightFences.at(Settings::currentFrame())));

	VkPresentInfoKHR presentInfo;
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &finished.at(Settings::currentFrame());
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &Settings::getSwapchain();
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;

	ASSERT_VK(vkQueuePresentKHR(Settings::getPresentQueue(), &presentInfo));

	Settings::currentFrame() = (Settings::currentFrame() + 1) % Settings::getMaxFramesInFlight();
}

void printMatrix(glm::mat4 matrix) {
	
	for (int j = 0; j < 4; j++) {
		for (int i = 0; i < 4; i++) {
			
			std::cout << matrix[i][j] << ", ";
		}
		std::cout << std::endl;
	}
}
 
void updateMVP() {
	
	auto frameTime = std::chrono::high_resolution_clock::now();

	float timeSinceStart = std::chrono::duration_cast<std::chrono::milliseconds>(frameTime - gameStartTime).count() / 1000.0f;
	// TODO Improvement?
	glm::mat4 model = glm::rotate(glm::mat4(1.0f), timeSinceStart * glm::radians(30.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	glm::mat4 view = glm::lookAt(glm::vec3(10.0f, 10.0f, 10.0f), glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	glm::mat4 projection = glm::perspective(glm::radians(60.0f), Settings::getSwapchainExtent().width / (float)Settings::getSwapchainExtent().height, 0.01f, 50.0f);
	projection[1][1] *= -1;
	
	ubo.model = model;
	ubo.view = view;
	ubo.proj = projection;
	ubo.lighPos = glm::vec3(0.0f, 30.0f, 10.0f);

	void* data;
	vkMapMemory(Settings::getDevice(), uniformBuffersMemory[Settings::currentFrame()], 0, sizeof(UniformBufferObject), 0, &data);
	memcpy(data, &ubo, sizeof(UniformBufferObject));
	vkUnmapMemory(Settings::getDevice(), uniformBuffersMemory[Settings::currentFrame()]);
}

void gameloop() {

	while (!glfwWindowShouldClose(Settings::getWindow())) {
		
		glfwPollEvents();
		
		drawFrame();
	}
}

void shutdownGLFW() {
	
	glfwDestroyWindow(Settings::getWindow());
	glfwTerminate();
}

void shutdownVulkan() {
	
	vkDeviceWaitIdle(Settings::getDevice());

	delete Settings::getDepthImage();

	vkDestroyDescriptorSetLayout(Settings::getDevice(), descriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(Settings::getDevice(), Settings::getDescriptorPool(), nullptr);

	for (uint32_t i = 0; i < amountImagesInSwapchain; ++i) {
		
		vkFreeMemory(Settings::getDevice(), uniformBuffersMemory[i], nullptr);
		vkDestroyBuffer(Settings::getDevice(), uniformBuffers[i], nullptr);
	}
	

	for (size_t i = 0; i < Settings::getMaxFramesInFlight(); i++) {
		
		vkDestroySemaphore(Settings::getDevice(), available.at(i), nullptr);
		vkDestroySemaphore(Settings::getDevice(), finished.at(i), nullptr);

		vkDestroyFence(Settings::getDevice(), inFlightFences.at(i), nullptr);
	}
	

	vkFreeMemory(Settings::getDevice(), indexBufferDeviceMemory, nullptr);
	vkDestroyBuffer(Settings::getDevice(), indexBuffer, nullptr);

	vkFreeMemory(Settings::getDevice(), vertexBufferDeviceMemory, nullptr);
	vkDestroyBuffer(Settings::getDevice(), vertexBuffer, nullptr);

	delete dragonMesh;
	delete debugTexture;
	Resources::releaseTextures();

	vkFreeCommandBuffers(Settings::getDevice(), Settings::getCommandPool(), amountImagesInSwapchain, commandBuffers.data());

	vkDestroyCommandPool(Settings::getDevice(), Settings::getCommandPool(), nullptr);

	for (size_t i = 0; i < amountImagesInSwapchain; i++) {
		
		vkDestroyFramebuffer(Settings::getDevice(), Settings::getFramebuffers().at(i), nullptr);
	}

	vkDestroyPipeline(Settings::getDevice(), pipeline, nullptr);
	vkDestroyRenderPass(Settings::getDevice(), renderPass, nullptr);
	vkDestroyPipelineLayout(Settings::getDevice(), pipelineLayout, nullptr);

	for (uint32_t i = 0; i < amountImagesInSwapchain; i++) {
		
		vkDestroyImageView(Settings::getDevice(), imageViews[i], nullptr);
	}

	vkDestroyShaderModule(Settings::getDevice(), shaderModuleVert, nullptr);
	vkDestroyShaderModule(Settings::getDevice(), shaderModuleFrag, nullptr);

	vkDestroySwapchainKHR(Settings::getDevice(), Settings::getSwapchain(), nullptr);
	vkDestroyDevice(Settings::getDevice(), nullptr);
	vkDestroySurfaceKHR(Settings::getInstance(), Settings::getSurface(), nullptr);
	
	destroyDebugUtilsMessengerEXT(debugMessenger, nullptr);
	vkDestroyInstance(Settings::getInstance(), nullptr);
}

int main() {

	Settings::init();
	Resources::init();

	startGLFW();
	startVulkan();
	
	gameloop();

	shutdownVulkan();
	shutdownGLFW();

	Resources::destroy();
	Settings::destroy();


	return 0;
}

