// ycoe_vba.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "zmq.h"
#include <string.h>
//#include <stdio.h>

#define MAX_MSGMAP_LEN 4096

#define COMMAND_FAILED -1
#define COMMAND_SUCCESS 1
#define COMMAND_ERROR 2

static int connection_active = 0;
static int profile_velocity = 0;
static int profile_acceleration = 0;
static int profile_deceleration = 0;
static int quick_stop_deceleration = 0;//QuickStopDeceleration
static int target_position = 0;
static int current_position = 0;
static unsigned char cmdmap[15];
static unsigned char msgmap[MAX_MSGMAP_LEN];


float _stdcall ycoe_vba_version(void)
{
	float version = 3.00;
	return version;
}


void *context;
void *requester;
int _stdcall ycoe_connect(char *data)
{
	connection_active = COMMAND_FAILED;
	context = zmq_ctx_new();
	requester = zmq_socket(context, ZMQ_REQ);
	zmq_connect(requester, "tcp://localhost:5555");
	connection_active = 1;
	return connection_active;
}
int _stdcall ycoe_disconnect(char *data)
{
	connection_active = COMMAND_FAILED;
	zmq_close(requester);
	zmq_ctx_destroy(context);
	connection_active = 0;
	return connection_active;
}


int _stdcall ycoe_speed(short slave, int velocity)
{
	if (connection_active) {
		cmdmap[0] = 36;
		cmdmap[1] = slave;
		profile_velocity = velocity;
		memcpy(cmdmap + 2, &profile_velocity, 4);
		//*(unsigned int *)(cmdmap + 2) = absolute_position;
		zmq_send(requester, cmdmap, 6, 0);
		zmq_recv(requester, msgmap, MAX_MSGMAP_LEN, 0);
		return profile_velocity;// COMMAND_SUCCESS;
	}
	else {
		return COMMAND_FAILED;
	}
}
int _stdcall ycoe_accel(short slave, int acceleration)
{
	if (connection_active) {
		unsigned char cmdmap[50];
		cmdmap[0] = 39;
		cmdmap[1] = slave;
		profile_acceleration = acceleration;
		memcpy(cmdmap + 2, &profile_acceleration, 4);
		zmq_send(requester, cmdmap, 6, 0);
		zmq_recv(requester, msgmap, MAX_MSGMAP_LEN, 0);
		return acceleration;// COMMAND_SUCCESS;
	}
	else {
		return COMMAND_FAILED;
	}
}
int _stdcall ycoe_set_motion_parameters(unsigned int velocity, unsigned int acceleration, unsigned int deceleration, unsigned int qs_deceleration)
{
	if (connection_active) {
		profile_velocity = velocity;
		profile_acceleration = acceleration;
		profile_deceleration = deceleration;
		quick_stop_deceleration = qs_deceleration;
		return COMMAND_SUCCESS;
	}
	else {
		return COMMAND_FAILED;
	}
}
int _stdcall ycoe_get_motion_parameters(unsigned int *velocity, unsigned int *acceleration, unsigned int *deceleration, unsigned int *qs_deceleration)
{
	if (connection_active) {
		*velocity = profile_velocity;
		*acceleration = profile_acceleration;
		*deceleration = profile_deceleration;
		*qs_deceleration = quick_stop_deceleration;
		return COMMAND_SUCCESS;
	}
	else {
		return COMMAND_FAILED;
	}
}



int _stdcall ycoe_abspos(short slave, int abspos)
{
	if (connection_active) {
		cmdmap[0] = 3;
		cmdmap[1] = slave;
		target_position = abspos;
		//unsigned char *pos = (unsigned char *)&target_position;
		//memcpy(cmdmap + 2, pos, 4);
		memcpy(cmdmap + 2, &target_position, 4);
		//*(unsigned int *)(cmdmap + 2) = absolute_position;
		zmq_send(requester, cmdmap, 6, 0);
		zmq_recv(requester, msgmap, MAX_MSGMAP_LEN, 0);
		return abspos;// COMMAND_SUCCESS;
	}
	else {
		return COMMAND_FAILED;
	}
}
int _stdcall ycoe_relpos(short slave, int relative_position)
{
	if (connection_active) {
		cmdmap[0] = 3;
		cmdmap[1] = slave;
		target_position += relative_position;
		//unsigned char *pos = (unsigned char *)&target_position;
		//memcpy(cmdmap + 2, pos, 4);
		memcpy(cmdmap + 2, &target_position, 4);
		//*(unsigned int *)(cmdmap + 2) = absolute_position;
		zmq_send(requester, cmdmap, 6, 0);
		zmq_recv(requester, msgmap, MAX_MSGMAP_LEN, 0);
		return target_position;// COMMAND_SUCCESS;
	}
	else {
		return COMMAND_FAILED;
	}
}
int _stdcall ycoe_tarpos(short slave)
{
	if (connection_active) {
		memset(cmdmap, 0, 15);
		zmq_send(requester, cmdmap, 6, 0);
		zmq_recv(requester, msgmap, 50, 0);

		int numslaves, ibytes, obytes;
		memcpy(&numslaves, msgmap, 4);
		memcpy(&ibytes, msgmap+4, 4);
		memcpy(&obytes, msgmap+8, 4);

		if (slave == 1)
			memcpy(&target_position, msgmap + 12+ibytes+2, 4);
		else if (slave == 2)
			memcpy(&target_position, msgmap + 12+ibytes+obytes+8+ibytes+2, 4);
		return target_position;
	}
	else {
		return COMMAND_FAILED;
	}
}
int _stdcall ycoe_curpos(short slave)
{
	if (connection_active) {
		memset(cmdmap, 0, 15);
		zmq_send(requester, cmdmap, 6, 0);
		zmq_recv(requester, msgmap, 50, 0);

		int numslaves, ibytes, obytes;
		memcpy(&numslaves, msgmap, 4);
		memcpy(&ibytes, msgmap + 4, 4);
		memcpy(&obytes, msgmap + 8, 4);

		if (slave == 1)
			memcpy(&current_position, msgmap + 12+2, 4);
		else if (slave == 2)
			memcpy(&current_position, msgmap + 12+ibytes+obytes+8+2, 4);
		return current_position;
	}
	else {
		return COMMAND_FAILED;
	}
}

int _stdcall ycoe_stopall(void)
{
  if (connection_active) {
 		cmdmap[0] = 43;
		zmq_send(requester, cmdmap, 1, 0);
		zmq_recv(requester, msgmap, MAX_MSGMAP_LEN, 0);
		return COMMAND_SUCCESS;
  } else {
		return COMMAND_FAILED;
  }
}

int _stdcall ycoe_toggledout(short slave, short ioport)
{
	if (connection_active) {
		cmdmap[0] = 23;
		cmdmap[1] = slave;
		cmdmap[2] = ioport;
		zmq_send(requester, cmdmap, 3, 0);
		zmq_recv(requester, msgmap, MAX_MSGMAP_LEN, 0);
		
		return COMMAND_SUCCESS;
	}
	else {
		return COMMAND_FAILED;
	}
}

int _stdcall ycoe_getdout(short slave) {
	cmdmap[0] = -1;
	zmq_send(requester, cmdmap, 3, 0);
	zmq_recv(requester, msgmap, 100, 0);

	int numslaves, ibytes, obytes;
	int frame_loc = 4;
	unsigned int dout1;
	memcpy(&numslaves, msgmap, 4);
	if (slave <= numslaves) {
		for (int i = 1; i < slave; i++) {
			memcpy(&ibytes, msgmap + frame_loc, 4);
			memcpy(&obytes, msgmap + frame_loc + 4, 4);
			frame_loc += (4 + 4 + ibytes + obytes);
		}
		memcpy(&ibytes, msgmap + frame_loc, 4);
		memcpy(&obytes, msgmap + frame_loc + 4, 4);
		memcpy(&dout1, msgmap + frame_loc + 8 + ibytes, 4);
	}
	return dout1;
}

int _stdcall ycoe_test(short slave, int abspos)
{
	if (connection_active) {
		cmdmap[0] = 3;
		cmdmap[1] = slave;
		memcpy(cmdmap + 2, &abspos, 4);
		zmq_send(requester, cmdmap, 6, 0);
		zmq_recv(requester, msgmap, MAX_MSGMAP_LEN, 0);
		return abspos;
	}
	else
		return slave;
}

static int ticker_int = 0;
int _stdcall ycoe_ticker(short arrsize, char arr[])
{
	if (connection_active) {
		cmdmap[0] = 198;
		cmdmap[1] = arrsize;
		cmdmap[2] = arrsize;
		zmq_send(requester, cmdmap, 3, 0);
		zmq_recv(requester, msgmap, MAX_MSGMAP_LEN, 0);

		memcpy(arr, msgmap, arrsize);
		
		//return COMMAND_SUCCESS;
		++ticker_int;
		return ticker_int;
	}
	else {
		memset(arr, -1, arrsize);
		return COMMAND_FAILED;
	}
}