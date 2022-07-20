
#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_proc.h"
#include "kernel_sched.h"
#include "kernel_cc.h"

int socket_read(void* ctrl_block, char *buf, unsigned int size) {
	
	SCB* socket= (SCB*) ctrl_block;
	
	PCB* pcb= CURPROC;
	if (accept_flag)
		pcb= CURPROC->parent;
	FCB* fcb= pcb->FIDT[socket->pe->receive];
	
	pipe_ctrl_block* pipe= fcb->streamobj;
	
	return pipe_read(pipe, buf, size);	
}

int socket_write(void* ctrl_block, const char* buf, unsigned int size) { 
	SCB* socket= (SCB*) ctrl_block;
	
	PCB* pcb= CURPROC;
	if (accept_flag)
		pcb= CURPROC->parent;
	FCB* fcb= pcb->FIDT[socket->pe->send];
	
	pipe_ctrl_block* pipe= fcb->streamobj;
	
	return pipe_write(pipe, buf, size);
}

int socket_close(void* ctrl_block) {

	SCB* socket= (SCB*) ctrl_block;

	if (socket== NULL)
		goto error_socket_close;

	if (socket->soc_t== LISTENER) {
		PORTS_TABLE[socket->port]= NULL;
		free(socket->lis);
	}
	
	if (socket->soc_t== PEER)
		free(socket->pe);
	
	free(socket);
	
	return 0; 

	error_socket_close:
		return -1;
}

file_ops Socket_fops = {
  .Open = NULL,
  .Read = socket_read,
  .Write = socket_write,
  .Close = socket_close
};


Fid_t Socket(port_t port) {

	FCB* fcb;
	Fid_t fid;

	Mutex_Lock(&kernel_mutex);

	PCB* pcb = CURPROC;

	if (accept_flag) 
		pcb= CURPROC->parent;

	if((port<0 || port>MAX_PORT))
		goto error_socket;

	if(!FCB_reserve(1, &fid, &fcb)) { // If the fids are exhausted 
		error_socket:
		Mutex_Unlock(&kernel_mutex);
		return NOFILE;
	}
	else {
		// Initialize a socket control block and link it to streamobj (of both fids)

		SCB* socket = (SCB*) xmalloc(sizeof(SCB));   

		socket->port= port;
		socket->soc_t= UNBOUND;
		socket->socket_fcb= fcb; //to be erased
		socket->parent=CURPROC;
		socket->sid= fid; //to be erased   
		socket->obj=NULL;   

		fcb->streamobj = socket ;
		fcb->streamfunc = &Socket_fops ;   
		   
		Mutex_Unlock(&kernel_mutex);	

		return fid ; 
	}
}

int Listen(Fid_t sock) {
	
	Mutex_Lock(&kernel_mutex);

	if (sock== NOFILE || sock< 0 || sock> MAX_FILEID)
		goto error_listen;

	if (PORTS_TABLE[sock]!= NULL)
		goto error_listen;

	FCB* fcb= get_fcb(sock);
	
	if (fcb== NULL)
		goto error_listen;

	SCB* socket=  fcb->streamobj;

	if (socket== NULL)
		goto error_listen;

	if (socket->port== NOPORT) 
		goto error_listen;

	if (PORTS_TABLE[socket->port]!= NULL)
		goto error_listen;

	if (socket->soc_t!= UNBOUND)
		goto error_listen;

	socket->soc_t=LISTENER;
	socket->lis=(listener*)xmalloc(sizeof(listener));
	socket->lis->requests=*(rlnode_init(&(socket->lis->requests), NULL));
	socket->lis->cv=COND_INIT;
	socket->lis->refcount= 0;
	PORTS_TABLE[socket->port]=socket;

	Mutex_Unlock(&kernel_mutex);
	return 0;
	
	error_listen:
	Mutex_Unlock(&kernel_mutex);
		return -1;
}


Fid_t Accept(Fid_t lsock) {

	Mutex_Lock(&kernel_mutex);

	if (lsock>MAX_FILEID-1 || lsock<0)
		goto error_accept_without_req;

	PCB* pcb= CURPROC;
	
	if (accept_flag) 
		pcb= CURPROC->parent;

	FCB* fcb= pcb->FIDT[lsock];
	
	if (fcb== NULL)
		goto error_accept_without_req;
	
	SCB* lsocket= fcb->streamobj;

	if (lsocket== NULL)
		goto error_accept_without_req;
	
	if (lsocket->soc_t!= LISTENER)
		goto error_accept_without_req;
	
	port_t port;
	int found=0;
	for (port=0; port<MAX_PORT; port++) {
		if (PORTS_TABLE[port]== lsocket) {
			found=1;
			break;
		}
	}
	
	if (!found)
		goto error_accept_without_req;

	SCB* listener= PORTS_TABLE[port];

	if (listener->lis->refcount== 0)
		Cond_Wait(&kernel_mutex, &(listener->lis->cv), 0);

	rlnode* node=(rlnode*)xmalloc(sizeof(rlnode));
	node= rlist_pop_front(&(listener->lis->requests));
	request* req=node->req;
	SCB* connecting_soc= req->socket;	
	
	if (connecting_soc->soc_t!= UNBOUND)
		goto error_accept_ref;
	
	SCB* connected_soc;

	Mutex_Unlock(&kernel_mutex);
	Fid_t fid= Socket(connecting_soc->port);
	Mutex_Lock(&kernel_mutex);
	
	if (fid==NOFILE)
		goto error_accept_ref;

	FCB* fcb1=get_fcb(fid);

	if (accept_flag)
		fcb1= pcb->FIDT[fid];

	if (fcb1== NULL)
		goto error_accept_ref;

	connected_soc= fcb1->streamobj;

	FCB_incref(connected_soc->socket_fcb);
	
	connected_soc->pe=(peer*)xmalloc(sizeof(peer));
	connecting_soc->pe=(peer*)xmalloc(sizeof(peer));
	
	pipe_t pipe1;
	pipe_t pipe2;

	//int tmp= accept_flag; //!!!
	Mutex_Unlock(&kernel_mutex);
	//accept_flag=0;//!!!
	
	if (Pipe(&pipe1)==-1 || Pipe(&pipe2)==-1) { 
		Mutex_Lock(&kernel_mutex);
		goto error_accept_ref;
	}
	//accept_flag= tmp;//!!!
	Mutex_Lock(&kernel_mutex);
	
	connected_soc->soc_t=PEER;
	connecting_soc->soc_t=PEER;
	connected_soc->pe->send=pipe1.write;
	connected_soc->pe->receive=pipe2.read;
	connecting_soc->pe->send=pipe2.write;
	connecting_soc->pe->receive=pipe1.read;
	listener->lis->refcount--;
	req->served=1;

	Cond_Signal(&req->cv);

	Mutex_Unlock(&kernel_mutex);
	return connected_soc->sid;

	error_accept_ref:
		listener->lis->refcount--;
		Cond_Signal(&req->cv);
	error_accept_without_req:
		Mutex_Unlock(&kernel_mutex);
		return NOFILE;
}


int Connect(Fid_t sock, port_t port, timeout_t timeout) {

	Mutex_Lock(&kernel_mutex);

	if (port> MAX_PORT || port< 0)
		goto error_connect;

	if (PORTS_TABLE[port]== NULL)
		goto error_connect;

	PCB* pcb= CURPROC;

	if (accept_flag) 
		pcb= CURPROC->parent;

	FCB* fcb= pcb->FIDT[sock];

	if (fcb== NULL)
		goto error_connect;

	SCB* socket= fcb->streamobj;

	if (socket== NULL)
		goto error_connect;

	if (socket->soc_t!= UNBOUND)
		goto error_connect;

	rlnode* node=(rlnode*)xmalloc(sizeof(rlnode));
	SCB* listener= PORTS_TABLE[port];
	request* req= (request*)xmalloc(sizeof(request));
	req->socket= socket;
	req->cv= COND_INIT;
	req->served= -1;
	node= rlnode_init(node, req);
	rlist_push_back(&listener->lis->requests, node);
	
	if (listener->lis->refcount== 0) 
		Cond_Broadcast(&listener->lis->cv);
	listener->lis->refcount++;

	Cond_Wait(&kernel_mutex, &req->cv, 0);

	if (req->served== -1)
		goto error_connect;

	Mutex_Unlock(&kernel_mutex);
	return 0;

	error_connect:
		Mutex_Unlock(&kernel_mutex);
		return -1;
}


int ShutDown(Fid_t sock, shutdown_mode how) {
	
	Mutex_Lock(&kernel_mutex);

	FCB* fcb= get_fcb(sock);

	if (fcb== NULL)
		goto error_shutdown;

	SCB* socket= (SCB*) fcb->streamobj;
	
	if (socket== NULL)
		goto error_shutdown;
	
	if (socket->soc_t!= PEER)
		goto error_shutdown; 

	FCB* fcbr= get_fcb(socket->pe->receive);
	FCB* fcbs= get_fcb(socket->pe->send);

	if (fcbr== NULL || fcbs== NULL)
		goto error_shutdown;
	
	pipe_ctrl_block* PipeCBreceive= fcbr->streamobj;
	pipe_ctrl_block* PipeCBsend= fcbs->streamobj;
	
	if (how== SHUTDOWN_READ)
		pipe_reader_close(PipeCBreceive);
	
	if (how== SHUTDOWN_WRITE)
		pipe_writer_close(PipeCBsend);	

	if (how== SHUTDOWN_BOTH) {
		pipe_reader_close(PipeCBreceive);
		pipe_writer_close(PipeCBsend);	
	}

	Mutex_Unlock(&kernel_mutex);
	return 0;
	
	error_shutdown:
		Mutex_Unlock(&kernel_mutex);
		return -1;
}
