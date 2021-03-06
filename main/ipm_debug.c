/** \file
 * \brief Yaskawa CoE master application
 *
 * Usage : yaskawaCoE [ifname1]
 * ifname is NIC interface, f.e. eth0
 *
 * (c)Gautam Manoharan 2017 - 2018
 */

#include <stdio.h>
#include <string.h>
#include "ethercat.h"
#include "zmq.h"

#include "yaskawacoe.h"
#include "ycoe_engine.h"

#ifndef _WIN32
#include <pthread.h>
#endif

#ifdef _WIN32
extern HANDLE ecat_mutex;
#else
extern pthread_mutex_t ecat_mutex;
#endif

extern int ycoestate;
int islaveindex, numslaves = 0;

char *network_datamap_ptr;
int network_datamap_size = 0;

int pos_cmd_sem[3] = {0,0,0}; // Position Command Semaphore
DINT final_position = 0;
//char  oloop, iloop;

OSAL_THREAD_FUNC control_engine(void *ptr) {
  printf("Control Engine starts...\n\r");
  while (ycoestate != YCOE_STATE_OPERATIONAL) osal_usleep(1000);
  printf("Control Logic starts...\n\r");
  while (1) {    // Control Logic
#ifdef _WIN32
    WaitForSingleObject(ecat_mutex, INFINITE);
#else
    pthread_mutex_lock(&ecat_mutex);
#endif

    for (islaveindex = 1; islaveindex <= numslaves; islaveindex++) {
                         //ycoe_printstatus(1);

                        if(ycoe_checkstatus(islaveindex,SW_SWITCHON_DISABLED)) {
                            ycoe_setcontrolword(islaveindex,CW_SHUTDOWN);
                        }
                        else if(ycoe_checkstatus(islaveindex,SW_RTSO))
                          ycoe_setcontrolword(islaveindex,CW_SWITCHON);
                        else if(ycoe_checkstatus(islaveindex,SW_SWITCHED_ON))
                        {
                            ycoe_setcontrolword(islaveindex,CW_ENABLEOP);
                            final_position = 181920;
                            pos_cmd_sem[islaveindex] = 1;
                            //ycoe_ipm_set_position (1,8192);
                        }
                        else {
                          if (ycoe_checkstatus(islaveindex,SW_OP_ENABLED))
                          {
/*                            if (pos_cmd_sem[islaveindex] > 0) {
                              ycoe_ipm_set_position(islaveindex, final_position);
                              pos_cmd_sem[islaveindex]--;
                              ycoe_setcontrolword(islaveindex,CW_ENABLEOP | CW_IPM_ENABLE);
                            }
*/                          if (ycoe_ipm_checkcontrol(islaveindex, CW_IPM_DISABLE) || \
                              (ycoe_ipm_checkstatus(islaveindex,SW_IPM_ACTIVE)==0)) {
                              ycoe_setcontrolword(islaveindex,CW_ENABLEOP | CW_IPM_ENABLE);
                            }
                            if (pos_cmd_sem[islaveindex] > 0) {
                              /* Add interpolation calculations */
                              //printf("cycle %d: pos_cmd_sem[islaveindex]>0\n\r",i);
//          					          printf("PDO cycle %4d, T:%"PRId64"\n\r", i, ec_DCtime);
                              if (ycoe_ipm_goto_position(islaveindex,final_position)) {
                                pos_cmd_sem[islaveindex]--;
                              }
          					         // printf("PDO cycle %4d, T:%"PRId64"\n\r", i, ec_DCtime);
                            }
                          }
                        }
                    }
#ifdef _WIN32
    ReleaseMutex(ecat_mutex);
#else
    pthread_mutex_unlock(&ecat_mutex);
#endif
    // Control Logic ends
    osal_usleep(1000);
  }

}
/* Server for talking to GUI Application */
OSAL_THREAD_FUNC controlserver(void *ptr) {

  while (1) {
    if (ycoestate == YCOE_STATE_PREOP) {
    for (islaveindex = 1; islaveindex <= 2; islaveindex++) {
  /* Check & Set Interpolation Mode Parameters */
                //ycoe_ipm_get_parameters(islaveindex);
                printf("Slave %x Index:Subindex %x:%x Content = %x\n\r",islaveindex,0x1602,2,ycoe_readCOparam(islaveindex, 0x1602, 2));
                ycoe_ipm_setup(islaveindex);
                printf("Slave %x Index:Subindex %x:%x Content = %x\n\r",islaveindex,0x1602,2,ycoe_readCOparam(islaveindex, 0x1602, 2));
                ycoe_set_mode_of_operation(islaveindex,INTERPOLATED_POSITION_MODE);
                printf("Slave %x Index:Subindex %x:%x Content = %x\n\r",islaveindex,0x1602,2,ycoe_readCOparam(islaveindex, 0x1602, 2));
                ycoe_ipm_set_parameters(islaveindex,1048576,1048576);
                //ycoe_ipm_get_parameters(islaveindex);
    }
    break;
    }
 osal_usleep(100);
    }


  switch_to_next_ycoestate();
  while (1) {
    if (ycoestate == YCOE_STATE_SAFEOP) {
      switch_to_next_ycoestate();
      break;
    }
    osal_usleep(100);
  }

  //  Socket to talk to clients
	void *context = zmq_ctx_new();
	void *responder = zmq_socket(context, ZMQ_REP);
	int rc = zmq_bind(responder, "tcp://*:5555");
	char buffer[15];

	while (1) {
    zmq_recv(responder, buffer, 15, 0);

#ifdef _WIN32
    WaitForSingleObject(ecat_mutex, INFINITE);
#else
    pthread_mutex_lock(&ecat_mutex);
#endif
    network_datamap_size = ycoe_get_datamap(&network_datamap_ptr);
    memcpy(&numslaves,network_datamap_ptr,4);

   // User Inputs
    if (buffer[0] == 3) {
      USINT *slaveaddr = (USINT *)(buffer + 1);
      DINT *targetposition = (DINT *)(buffer + 1+1);
      if (*slaveaddr <= numslaves) {
        //ycoe_ipm_set_position(*slaveaddr, *targetposition);//Vulnerable to racing conditions
        final_position = *targetposition;
        pos_cmd_sem[*slaveaddr]++;
        printf("Slave %x Requested position:%d and pos_cmd_sem=%d\n\r",*slaveaddr,*targetposition,pos_cmd_sem[*slaveaddr]);
      } else {
        printf("Invalid slave address:%x Requested position:%d and pos_cmd_sem=%d\n\r",*slaveaddr,*targetposition,pos_cmd_sem[*slaveaddr]);
      }
    }
    else if (buffer[0] == 6) {
      USINT *slaveaddr = (USINT *)(buffer + 1);
      INT *regaddr = (INT *)(buffer + 1+1);
      printf("Slave %x Register %x Content = %x\n\r",*slaveaddr,*regaddr,ycoe_readreg_dint(*slaveaddr, *regaddr));
    }
    else if (buffer[0] == 9) {
      USINT *slaveaddr = (USINT *)(buffer + 1);
      INT *index = (INT *)(buffer + 1+1);
      INT *subindex = (INT *)(buffer + 1+1+4);
      printf("Slave %x Index:Subindex %x:%x Content = %x\n\r",*slaveaddr,*index,*subindex,ycoe_readCOparam(*slaveaddr, *index, *subindex));
    }
    else if (buffer[0] == 33) {
      USINT slaveaddr;
      DINT *targetposition = (DINT *)(buffer + 1);
      final_position = *targetposition;
      for (slaveaddr = 1; slaveaddr <= numslaves; slaveaddr++) {
        //ycoe_ipm_set_position(*slaveaddr, *targetposition);//Vulnerable to racing conditions
        pos_cmd_sem[slaveaddr]++;
        printf("Slave %x Requested position:%d and pos_cmd_sem=%d\n\r",slaveaddr,*targetposition,pos_cmd_sem[slaveaddr]);
      }
    }
    // User input ends

    zmq_send(responder, network_datamap_ptr, network_datamap_size, 0);
#ifdef _WIN32
    ReleaseMutex(ecat_mutex);
#else
    pthread_mutex_unlock(&ecat_mutex);
#endif
  }
}

int main(int argc, char *argv[])
{
  OSAL_THREAD_HANDLE thread1, thread2;
#ifdef _WIN32
  ecat_mutex = CreateMutex(
      NULL,              // default security attributes
      FALSE,             // initially not owned
      NULL);             // unnamed mutex
  if (ecat_mutex == NULL)
  {
    printf("CreateMutex error: %d\n", GetLastError());
    return 1;
  }
#else
  //ecat_mutex = PTHREAD_MUTEX_INITALIZER;
  pthread_mutex_init(&ecat_mutex, NULL);
#endif
  printf("YaskawaCoE (Yaskawa Canopen over Ethercat Master)\nControl Application\n");

  if (argc > 1)
  {
   // thread to handle gui application requests
    //osal_thread_create(&thread2, 128000, &controlserver, (void*)&ctime);
    /* start cyclic part */
    osal_thread_create(&thread1, 128000, &ycoe_engine, argv[1]);
    osal_thread_create(&thread2, 128000, &control_engine, argv[1]);
    //coeController(argv[1]);
    controlserver(argv[1]);
  }
  else
  {
    printf("Usage: yaskawaCoE ifname1\nifname = eth0 for example\n");
  }

#ifdef _WIN32
  CloseHandle(ecat_mutex);
#else
  pthread_mutex_destroy(&ecat_mutex);
#endif
  printf("End program\n");
  return (0);
}
