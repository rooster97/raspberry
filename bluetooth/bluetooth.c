#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <bluetooth/rfcomm.h>
#include <sys/wait.h>
#include <pthread.h>
#include <signal.h>
#include <wiringPi.h>
#include <syslog.h>

#define RASPBERRY_MAX_CLIENT_NUM			10
#define RASPBERRY_DEFAULT_LIMIT_DISTANCE	50
#define RASPBERRY_LIMIT_DISTANCE_STRING		"limit distance:"
#define RASPBERRY_GPIO_TRIG					2
#define RASPBERRY_GPIO_ECHO					8

int numClient=0;
int limitDistance;
int limitStringLen;
int clientSock[RASPBERRY_MAX_CLIENT_NUM];

void *ThreadMain(void *argument);
bdaddr_t bdaddr_any = {0, 0, 0, 0, 0, 0};
bdaddr_t bdaddr_local = {0, 0, 0, 0xff, 0xff, 0xff};

int _str2uuid( const char *uuid_str, uuid_t *uuid) 
{
	/* This is from the pybluez stack */
	uint32_t uuid_int[4];
	char *endptr;

	if( strlen( uuid_str ) == 36 ) 
	{
		char buf[9] = { 0 };

		if( uuid_str[8] != '-' && uuid_str[13] != '-' &&
				uuid_str[18] != '-' && uuid_str[23] != '-' ) {
			return 0;
		}
		// first 8-bytes
		strncpy(buf, uuid_str, 8);
		uuid_int[0] = htonl( strtoul( buf, &endptr, 16 ) );
		if( endptr != buf + 8 ) return 0;
		// second 8-bytes
		strncpy(buf, uuid_str+9, 4);
		strncpy(buf+4, uuid_str+14, 4);
		uuid_int[1] = htonl( strtoul( buf, &endptr, 16 ) );
		if( endptr != buf + 8 ) return 0;

		// third 8-bytes
		strncpy(buf, uuid_str+19, 4);
		strncpy(buf+4, uuid_str+24, 4);
		uuid_int[2] = htonl( strtoul( buf, &endptr, 16 ) );
		if( endptr != buf + 8 ) return 0;

		// fourth 8-bytes
		strncpy(buf, uuid_str+28, 8);
		uuid_int[3] = htonl( strtoul( buf, &endptr, 16 ) );
		if( endptr != buf + 8 ) return 0;

		if( uuid != NULL ) sdp_uuid128_create( uuid, uuid_int );
	} 
	else if ( strlen( uuid_str ) == 8 ) 
	{
		// 32-bit reserved UUID
		uint32_t i = strtoul( uuid_str, &endptr, 16 );
		if( endptr != uuid_str + 8 ) return 0;
		if( uuid != NULL ) sdp_uuid32_create( uuid, i );
	} 
	else if( strlen( uuid_str ) == 4 ) 
	{
		// 16-bit reserved UUID
		int i = strtol( uuid_str, &endptr, 16 );
		if( endptr != uuid_str + 4 ) return 0;
		if( uuid != NULL )
			sdp_uuid16_create( uuid, i
					);
	} 
	else 
	{
		return 0;
	}

	return 1;

}

sdp_session_t *register_service(uint8_t rfcomm_channel) {

	/* A 128-bit number used to identify this service. The words are ordered from most to least
	 * significant, but within each word, the octets are ordered from least to most significant.
	 * For example, the UUID represneted by this array is 00001101-0000-1000-8000-00805F9B34FB. (The
	 * hyphenation is a convention specified by the Service Discovery Protocol of the Bluetooth Core
	 * Specification, but is not particularly important for this program.)
	 *
	 * This UUID is the Bluetooth Base UUID and is commonly used for simple Bluetooth applications.
	 * Regardless of the UUID used, it must match the one that the Armatus Android app is searching
	 * for.
	 */
	const char *service_name = "Armatus Bluetooth server";
	const char *svc_dsc = "A HERMIT server that interfaces with the Armatus Android app";
	const char *service_prov = "Armatus";

	uuid_t root_uuid, l2cap_uuid, rfcomm_uuid, svc_uuid,
		   svc_class_uuid;
	sdp_list_t *l2cap_list = 0,
			   *rfcomm_list = 0,
			   *root_list = 0,
			   *proto_list = 0,
			   *access_proto_list = 0,
			   *svc_class_list = 0,
			   *profile_list = 0;
	sdp_data_t *channel = 0;
	sdp_profile_desc_t profile;
	sdp_record_t record = { 0 };
	sdp_session_t *session = 0;

	// set the general service ID
	//sdp_uuid128_create(&svc_uuid, &svc_uuid_int);
	_str2uuid("00001101-0000-1000-8000-00805F9B34FB",&svc_uuid);
	sdp_set_service_id(&record, svc_uuid);

	char str[256] = "";
	sdp_uuid2strn(&svc_uuid, str, 256);
	printf("Registering UUID %s\n", str);
	syslog(LOG_NOTICE|LOG_USER, "Registering UUID %s\n", str);

	// set the service class
	sdp_uuid16_create(&svc_class_uuid, SERIAL_PORT_SVCLASS_ID);
	svc_class_list = sdp_list_append(0, &svc_class_uuid);
	sdp_set_service_classes(&record, svc_class_list);

	// set the Bluetooth profile information
	sdp_uuid16_create(&profile.uuid, SERIAL_PORT_PROFILE_ID);
	profile.version = 0x0100;
	profile_list = sdp_list_append(0, &profile);
	sdp_set_profile_descs(&record, profile_list);

	// make the service record publicly browsable
	sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP);
	root_list = sdp_list_append(0, &root_uuid);
	sdp_set_browse_groups(&record, root_list);

	// set l2cap information
	sdp_uuid16_create(&l2cap_uuid, L2CAP_UUID);
	l2cap_list = sdp_list_append(0, &l2cap_uuid);
	proto_list = sdp_list_append(0, l2cap_list);

	// register the RFCOMM channel for RFCOMM sockets
	sdp_uuid16_create(&rfcomm_uuid, RFCOMM_UUID);
	channel = sdp_data_alloc(SDP_UINT8, &rfcomm_channel);
	rfcomm_list = sdp_list_append(0, &rfcomm_uuid);
	sdp_list_append(rfcomm_list, channel);
	sdp_list_append(proto_list, rfcomm_list);

	access_proto_list = sdp_list_append(0, proto_list);
	sdp_set_access_protos(&record, access_proto_list);

	// set the name, provider, and description
	sdp_set_info_attr(&record, service_name, service_prov, svc_dsc);

	// connect to the local SDP server, register the service record,
	// and disconnect
	session = sdp_connect(&bdaddr_any, &bdaddr_local, SDP_RETRY_IF_BUSY);
	sdp_record_register(session, &record, 0);

	// cleanup
	sdp_data_free(channel);
	sdp_list_free(l2cap_list, 0);
	sdp_list_free(rfcomm_list, 0);
	sdp_list_free(root_list, 0);
	sdp_list_free(access_proto_list, 0);
	sdp_list_free(svc_class_list, 0);
	sdp_list_free(profile_list, 0);

	return session;
}

char *read_msg_from_bluetooth (int sock, int i) 
{
	char buff[1024] = {0, };
	char buff2[1024] = {0, };

	// read data from the client
	int bytes_read;
	bytes_read = read(sock, buff, sizeof(buff));
	if (bytes_read > 0) 
	{
		printf("received : %s\n", buff);
		syslog(LOG_NOTICE|LOG_USER, "received : %s\n", buff);
		if (strncmp (buff, RASPBERRY_LIMIT_DISTANCE_STRING, limitStringLen) == 0)
		{	
			strcpy (buff2, buff + limitStringLen);
			limitDistance = atoi (buff2);
			printf("[Set] Limit Distance %d \n", limitDistance);
			syslog(LOG_NOTICE|LOG_USER, "[Set] Limit Distance %d \n", limitDistance);
		}
	} 
	else 
	{
		close (sock);	
		if (i != numClient-1)
			clientSock[i] = clientSock[numClient-1];
		
		numClient--;
		printf("[%s] read message error : %s", __func__, strerror(errno)); // TODO : check
		syslog(LOG_ERR|LOG_USER, "[%s] read message error : %s", __func__, strerror(errno));
	}
}

void write_msg_in_bluetooth (int sock, char *message)
{
	int bytes_sent;

	bytes_sent = write(sock, message, strlen(message));
	if (bytes_sent > 0) 
	{
		printf("sent : %s ", message);
		//syslog(LOG_NOTICE|LOG_USER, "sent : %s ", message);
	}
}

int get_max_fd (int connSock)
{
	int i;
	int max = connSock;

	for (i = 0; i < numClient; i++)
	{
		if (clientSock[i] > max)
			max = clientSock[i];
	}

	return max;
}

void check_distance_by_mircrowave ()
{
	int i, count;	
	int distance;
	int start_time, end_time;
	char buff[128] = "";

	pinMode(RASPBERRY_GPIO_TRIG, OUTPUT);
	pinMode(RASPBERRY_GPIO_ECHO, INPUT);

	digitalWrite(RASPBERRY_GPIO_TRIG, LOW);  // Init Trigger
	digitalWrite(RASPBERRY_GPIO_TRIG, HIGH) ;	// send signal
	delayMicroseconds(10) ;		// send signal for 10 usec
	digitalWrite(RASPBERRY_GPIO_TRIG, LOW) ;	// stop signal

	count = 0;
	while (digitalRead(RASPBERRY_GPIO_ECHO) == 0)
	{
		count++;
		if (count > 2500000) // Abount 1/2 sec. Prepare for absence of microwave sensor.
			break;
	}
	start_time = micros() ;	// get microseconds after program runs

	count = 0;
	while (digitalRead(RASPBERRY_GPIO_ECHO) == 1)
	{
		count++;
		if (count > 2500000) // abount 1/2 sec
			break;
	}
	end_time = micros() ;

	distance = (end_time - start_time) / 29 / 2 ;	// Ultrasound is 1cm per 29 microsecond, 2 is roundtrip.
	
	if (distance < limitDistance)
	{
		sprintf (buff, "distance %d cm \n", distance);
		for (i = 0; i < numClient; i++)
			write_msg_in_bluetooth (clientSock[i], buff);	
	}
}

int main()
{
	int i, ret;
	int maxFd;
	int connSock;
	struct timeval tv;
	pthread_t thread_id;  
	int port = 3, result, sock;
	struct sockaddr_rc loc_addr = { 0 }, rem_addr = { 0 };
	socklen_t opt = sizeof(rem_addr);

	fd_set read_fds;

	signal( SIGPIPE, SIG_IGN );  

	if (wiringPiSetup() == -1)
	{
		printf ("[%s:%d] fail to call wiringPiSetup function: %s \n", __func__, __LINE__, strerror(errno));
		syslog(LOG_ERR|LOG_USER, "[%s:%d] fail to call wiringPiSetup function: %s\n", 
				__func__, __LINE__, strerror(errno));

		exit(1) ;
	}

	limitDistance = RASPBERRY_DEFAULT_LIMIT_DISTANCE;
	limitStringLen = strlen(RASPBERRY_LIMIT_DISTANCE_STRING);

	// local bluetooth adapter
	loc_addr.rc_family = AF_BLUETOOTH;
	loc_addr.rc_bdaddr = bdaddr_any;
	loc_addr.rc_channel = (uint8_t) port;

	// register service
	sdp_session_t *session = register_service(port);
	
	// allocate socket
	sock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
	printf("socket() returned %d\n", sock);
	syslog(LOG_NOTICE|LOG_USER, "socket() returned %d\n", sock);

	// bind socket to port 3 of the first available
	result = bind(sock, (struct sockaddr *)&loc_addr, sizeof(loc_addr));
	printf("bind() on channel %d returned %d\n", port, result);
	syslog(LOG_NOTICE|LOG_USER, "bind() on channel %d returned %d\n", port, result);

	// put socket into listening mode
	result = listen(sock, RASPBERRY_MAX_CLIENT_NUM);
	printf("listen() returned %d\n", result);
	syslog(LOG_NOTICE|LOG_USER, "listen() returned %d\n", result);

	while(1)
	{
		/* FD set */
		maxFd = get_max_fd (sock) + 1; 

		FD_ZERO(&read_fds);
		FD_SET(sock, &read_fds);

		for (i = 0; i < numClient; i++)
			FD_SET(clientSock[i], &read_fds);

		/* set select interval. */
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		ret = select (maxFd, &read_fds, NULL, NULL, &tv);
		if (ret < 0) // Error 
		{    
			if (errno == EINTR)
				continue;

			printf ("[%s] Select error : %s", __func__, strerror(errno));
			syslog(LOG_ERR|LOG_USER, "[%s] Select error : %s", __func__, strerror(errno));

			continue;
		}    
		else if (ret == 0) // Timeout
		{
			check_distance_by_mircrowave ();	
		}
		else // Receive Message
		{
			if (FD_ISSET (sock ,&read_fds))
			{
				connSock = accept(sock, (struct sockaddr *)&rem_addr, &opt);
				printf("accept() returned %d\n", connSock);
				syslog(LOG_NOTICE|LOG_USER, "accept() returned %d\n", connSock);
			
				clientSock[numClient++] = connSock;
			}
		
			for (i = 0; i < numClient; i++)
			{
				if (FD_ISSET (clientSock[i] ,&read_fds))
				{
					read_msg_from_bluetooth (clientSock[i], i);		
				}
			}
		}
	}
}
