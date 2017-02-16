#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <libxml/tree.h>
#include "nvml.h"
#include <signal.h>

void http(int sockfd);
int send_msg(int fd, char *msg);

xmlNodePtr create_node(unsigned char *node_name, unsigned char *text)
{
	xmlNodePtr new_node = NULL;
	xmlNodePtr new_text = NULL;
	new_node = xmlNewNode(NULL, node_name);
	if (text != NULL) {
		new_text = xmlNewText(text);
		xmlAddChild(new_node, new_text);
	}
	return new_node;
}

#define NODE_TYPE_INT 1
#define NODE_TYPE_CHAR 2
#define NODE_TYPE_FLOAT 3
#define NODE_TYPE_DOUBLE 4
#define NODE_TYPE_NULL 0

xmlNodePtr addNode(xmlNodePtr parent_node, char *name, void *data, int type)
{
	xmlNodePtr child_node;
	char buf[64];
	

	switch(type){
		case NODE_TYPE_INT:
			sprintf(buf,"%d", *(int*)data);
			break;
		case NODE_TYPE_CHAR:
			sprintf(buf,"%s", (char*)data);
			break;
		case NODE_TYPE_FLOAT:
			sprintf(buf,"%f", *(float*)data);
			break;
		case NODE_TYPE_DOUBLE:
			sprintf(buf,"%f", *(double*)data);
			break;
		case NODE_TYPE_NULL:
		default:
			memset(buf, 0, sizeof(buf));
	}
	child_node = create_node(name, buf);
	xmlAddChild(parent_node, child_node);
}

static char output_file[] = "./gpu_status.xml";

int create_xml(unsigned char **ret) {

	// for nvml
	unsigned int nvmlDeviceCount;
	int memUsed, memTotal;
	unsigned int power_mill;
	double power;
	int i,j;
	unsigned int infoCount;
	nvmlProcessInfo_t *infos;

	infos = (nvmlProcessInfo_t *)malloc(sizeof(nvmlProcessInfo_t) * 32);

	nvmlDevice_t nvmlDevice;
	nvmlMemory_t nvmlMemory;
	nvmlPstates_t nvmlpState;
	nvmlPciInfo_t pciInfo;
	nvmlBrandType_t nvmltype; 

	//
	char hostname[32];
	char buf[32];
	char procname[32];

	xmlDocPtr doc;
	xmlNodePtr root_node;
	xmlNodePtr platform_node;
	xmlNodePtr device_node;
	xmlNodePtr obj_node;
	
	gethostname(hostname, 31);

	doc = xmlNewDoc("1.0"); 
	root_node = create_node("Host", NULL); 
	xmlNewProp(root_node, "Name", hostname);
	xmlDocSetRootElement(doc, root_node); 

	platform_node = create_node("CUDA", NULL);
	xmlAddChild(root_node, platform_node);
	
	nvmlInit();
	nvmlSystemGetDriverVersion(buf, 31);
	addNode(platform_node, "Driver version", (void*)buf, NODE_TYPE_CHAR);
	nvmlSystemGetNVMLVersion(buf, 31);
	addNode(platform_node, "NVML version", (void*)buf, NODE_TYPE_CHAR);
	
	nvmlDeviceGetCount(&nvmlDeviceCount);
	for(i=0;i<nvmlDeviceCount;i++){
		sprintf(buf,"GPU#%d", i);
		device_node = create_node(buf, NULL);
		xmlAddChild(platform_node, device_node); 
		
		nvmlDeviceGetHandleByIndex(i, &nvmlDevice);

		nvmlDeviceGetName ( nvmlDevice, buf, 31 );
		addNode(device_node, "Board Name", (void*)buf, NODE_TYPE_CHAR);
		
		obj_node = create_node("PCIe", NULL);
		xmlAddChild(device_node, obj_node); 
		nvmlDeviceGetPciInfo(nvmlDevice, &pciInfo);
		addNode(obj_node, "busId", (void*)pciInfo.busId, NODE_TYPE_CHAR);

		nvmlDeviceGetBrand ( nvmlDevice, &nvmltype);
#ifndef NVML_BRAND_GEFORCE
#define NVML_BRAND_GEFORCE 5
#endif
		switch(nvmltype){
			case NVML_BRAND_TESLA:
				sprintf(buf, "TESLA");
				break;
			case NVML_BRAND_GEFORCE:
				sprintf(buf, "GEFORCE");
				break;
			case NVML_BRAND_GRID:
				sprintf(buf, "GRID");
				break;
			default:
				memset(buf, 0, sizeof(buf));
		}
		addNode(device_node, "Brand", (void*)&buf, NODE_TYPE_CHAR);

		nvmlUtilization_t nvmlUtil;
		nvmlDeviceGetUtilizationRates ( nvmlDevice, &nvmlUtil);
		
		obj_node = create_node("Utilization", NULL);
		xmlAddChild(device_node, obj_node); 
		addNode(obj_node, "GPU", (void*)&nvmlUtil.gpu, NODE_TYPE_INT);
		addNode(obj_node, "Memory", (void*)&nvmlUtil.memory, NODE_TYPE_INT);



		nvmlDeviceGetPowerUsage ( nvmlDevice, &power_mill );
		power = (float)power_mill / 1000.0;
		addNode(device_node, "powerUsage", (void*)&power, NODE_TYPE_DOUBLE);

		obj_node = create_node("Memory", NULL);
		xmlAddChild(device_node, obj_node); 

		nvmlDeviceGetMemoryInfo(nvmlDevice, &nvmlMemory);
		memUsed = nvmlMemory.used / 1024 / 1024;
		addNode(obj_node, "memUsed", (void*)&memUsed, NODE_TYPE_INT);
		memTotal = nvmlMemory.total / 1024 / 1024;
		addNode(obj_node, "memTotal",(void*) &memTotal, NODE_TYPE_INT);

		nvmlDeviceGetComputeRunningProcesses(nvmlDevice, &infoCount, NULL);
		if(infoCount){
			nvmlDeviceGetComputeRunningProcesses(nvmlDevice, &infoCount, infos );
			for(j=0; j<infoCount; j++)
			{
				obj_node = create_node("Process", NULL);
				xmlNewProp(obj_node, "TYPE", "Compute"); 
				xmlAddChild(device_node, obj_node);

				addNode(obj_node, "PID", &infos[j].pid, NODE_TYPE_INT);
				nvmlSystemGetProcessName(infos[j].pid, procname , 31);
				addNode(obj_node, "ProcessName", procname, NODE_TYPE_CHAR);
				infos[j].usedGpuMemory = infos[j].usedGpuMemory / 1024 / 1024;
				addNode(obj_node, "usedGpuMemory", &infos[j].usedGpuMemory, NODE_TYPE_INT);
			}
		}

		nvmlDeviceGetGraphicsRunningProcesses(nvmlDevice, &infoCount, NULL);
		if(infoCount){
			nvmlDeviceGetGraphicsRunningProcesses(nvmlDevice, &infoCount, &infos[0] );
			for(j=0; j<infoCount; j++)
			{
				obj_node = create_node("Process", NULL);
				xmlNewProp(obj_node, "TYPE", "Graphic");
				xmlAddChild(device_node, obj_node); 

				addNode(obj_node, "PID", &infos[j].pid, NODE_TYPE_INT);
				nvmlSystemGetProcessName(infos[j].pid, procname , 31);
				addNode(obj_node, "ProcessName", procname, NODE_TYPE_CHAR);
				infos[j].usedGpuMemory = infos[j].usedGpuMemory / 1024 / 1024;
				addNode(obj_node, "usedGpuMemory", &infos[j].usedGpuMemory, NODE_TYPE_INT);
			}

		}
		
#if 0
		sprintf(buf,"%d", memUsed);
		obj_node = create_node("memUsed", buf);
		xmlAddChild(device_node, obj_node);
		
		memTotal = nvmlMemory.total / 1024 / 1024;
		sprintf(buf,"%d", memTotal);
		obj_node = create_node("memTotal", buf);
		xmlAddChild(device_node, obj_node);
#endif	
	}
	nvmlShutdown();

	xmlChar * xml_buf;
	int size;
	xmlDocDumpMemoryEnc(doc, &xml_buf, &size, "UTF-8"); 
	*ret = strdup(xml_buf);
	printf("%s\n", *ret,ret);
	xmlSaveFormatFileEnc(output_file, doc, "UTF-8", 1);

	xmlFreeDoc(doc);
	xmlFree(xml_buf);
}

int sockfd;
int new_sockfd;

void sigint_action(int signum)
{
	close(new_sockfd);
	close(sockfd);
	exit(0);
}


int start_server(int port) {

   	struct sigaction action;

	memset(&action, 0, sizeof(action));
	action.sa_handler = sigint_action;
	action.sa_flags = SA_RESETHAND;   
	if (sigaction(SIGINT, &action, NULL) != 0) {
		perror("sigaction");
		return -1;
	}
	printf("Waiting for request...[%d]\n", port);

	int writer_len;

	struct sockaddr_in reader_addr;
	struct sockaddr_in writer_addr;

	memset(&reader_addr, 0, sizeof(reader_addr));
	memset(&writer_addr, 0, sizeof(writer_addr));

	reader_addr.sin_family      = AF_INET;
	reader_addr.sin_addr.s_addr = htonl(INADDR_ANY); 
	reader_addr.sin_port        = htons(port);

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "error: socket()\n");
		return 1;
	}

	if (bind(sockfd, (struct sockaddr *)&reader_addr, sizeof(reader_addr)) < 0) {
		fprintf(stderr, "error: bind()\n");
		fprintf(stderr, "%s", strerror(errno));
		close(sockfd);
		return 1;
	}

	if (listen(sockfd, 5) < 0) {
		fprintf(stderr, "error: listen()\n");
		close(sockfd);
		return 1;
	}

	while(1) {
		if ((new_sockfd = accept(sockfd, (struct sockaddr *)&writer_addr, (socklen_t *)&writer_len)) < 0) {
			fprintf(stderr, "error: accepting a socket.\n");
			break;
		}
		else {
			http(new_sockfd);
			close(new_sockfd);
		}
	}

	close(sockfd);
}


void http(int sockfd) {

	int len;
	int read_fd;
	char buf[1024];
	unsigned char *xml_buf;
	char method[16];
	char uri_addr[256];
	char http_ver[64];
	char *uri_file;

	if (read(sockfd, buf, 1024) <= 0) {
		fprintf(stderr, "error: reading a request.\n");
	}
	else {
		sscanf(buf, "%s %s %s", method, uri_addr, http_ver);

		if (strcmp(method, "GET") != 0) {
			send_msg(sockfd, "501 Not implemented.");
			close(read_fd);
		}


		uri_file = uri_addr + 1;

		send_msg(sockfd, "HTTP/1.0 200 OK\r\n");
		send_msg(sockfd, "Content-Type: text/html\r\n");
		send_msg(sockfd, "\r\n");
	
		//xml 
		create_xml(&xml_buf);
	
		if ((read_fd = open(output_file, O_RDONLY, 0666)) == -1) {
			send_msg(sockfd, "404 Not Found");
			close(read_fd);
		}

		while((len = read(read_fd, buf, 1024)) > 0) {
			if (write(sockfd, buf, len) != len) {
				fprintf(stderr, "error: writing a response.\n");
				break;
			}
		}



	//	write(sockfd, xml_buf, strlen(xml_buf));

		close(read_fd);
	}
}

int send_msg(int fd, char *msg) {
	int len;
	len = strlen(msg);

	if (write(fd, msg, len) != len) {
		fprintf(stderr, "error: writing.");
	}

	return len;
}
