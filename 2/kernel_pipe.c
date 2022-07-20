
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_streams.h"
#include "kernel_cc.h"
#include "kernel_dev.h"
#include "util.h"
#include "kernel_proc.h"

int read_first_flag=0;

int dummy ()
{
	return -1 ; 
}

int pipe_read(void* ctrl_block, char *buf, unsigned int size) {
	
	read_first_flag=1;

	pipe_ctrl_block *pipe_ctrl = (pipe_ctrl_block*) ctrl_block;
	
	preempt_on ; 
	Mutex_Lock(& pipe_ctrl->mut);

	int count =  0; // Αριθμός των char που διαβάσαμε

	if(pipe_ctrl->reader== NULL) {
		Mutex_Unlock(& pipe_ctrl->mut);
		preempt_off; 
		read_first_flag=0;
		return -1;
	}

	if (write_flag == 1) Cond_Broadcast(&(pipe_ctrl->space_var)) ; 

 	if ((pipe_ctrl->numOfElements == 0) && (pipe_ctrl->writer !=NULL) && (write_flag == 0)) {// Οταν δεν υπάρχουν δεδομένα and write is open , η read θα κοιμάται
	   Cond_Broadcast(&(pipe_ctrl->space_var)) ;
	   Cond_Wait(&pipe_ctrl->mut, &pipe_ctrl->data_var,0);
	}     

	if ((pipe_ctrl->numOfElements == 0 ) && (write_flag == 0)) goto end_read;
 
  	while(count<size) {
		buf[count] = pipe_ctrl->buffer[pipe_ctrl->head] ; // Μεταφέρουμε τα δεδομένα απο το pipe στο εξωτερικο buffer
		count++; 

  		// Υπολογισμός νέου head . numberofElements
  		pipe_ctrl->head = (pipe_ctrl->head + 1 ) % BUF_SIZE ; // Εξασφαλίζει οτι το head είναι πάντα μέσα στα όρια του array

    	if (pipe_ctrl->numOfElements >= 0 ) pipe_ctrl->numOfElements-- ; 

    	if ( pipe_ctrl->numOfElements == 0 ) {
    		if (write_flag == 1) { 
				Cond_Broadcast(&(pipe_ctrl->space_var)) ; 
				Cond_Wait(&pipe_ctrl->mut, &pipe_ctrl->data_var,0);
			}
		else goto end_read;
		} 		

	}

	end_read:
		Mutex_Unlock(& pipe_ctrl->mut);
		preempt_off; 
		read_first_flag=0;
		return count; // Returns the number of bytes/chars it read
}

int pipe_write(void* ctrl_block, const char* buf, unsigned int size) { // like serial_write
	
	pipe_ctrl_block *pipe_ctrl = (pipe_ctrl_block*) ctrl_block;

	preempt_on; 
	Mutex_Lock(& pipe_ctrl->mut);

	int count =  0; // Αριθμός των char που γράψαμε
	
  	if (pipe_ctrl->writer == NULL) {
		Mutex_Unlock(& pipe_ctrl->mut);
		preempt_off; 
		return -1;
	}
    if (pipe_ctrl->reader == NULL) {// Read end is closed, so write becomes unusable
    	Mutex_Unlock(& pipe_ctrl->mut);
		preempt_off; 
		return -1; 
    }

	while(count<size) {
		int write_position =  (pipe_ctrl->head + pipe_ctrl->numOfElements) % BUF_SIZE; 
	    pipe_ctrl->buffer[write_position] = buf[count] ; // Μεταφέρουμε τα δεδομένα απο το εξωτερικο buffer στο pipe    
		// Υπολογισμός νέου numberofElements
    	pipe_ctrl->numOfElements++ ; 
 		count++;     

		if ((pipe_ctrl->numOfElements == BUF_SIZE) && (pipe_ctrl->reader!=NULL) && (count != size) ) { 
			Cond_Broadcast (&(pipe_ctrl->data_var)) ; // Υπάρχουν δεδομένα για ανάγνωση , αρα ξυπνα την read
			write_flag = 1 ; 
    		Cond_Wait(&pipe_ctrl->mut, &pipe_ctrl->space_var,0);  
    	}   
	}
	write_flag = 0 ; 
	if (read_first_flag) 
		Cond_Broadcast (&(pipe_ctrl->data_var)); // Υπάρχουν δεδομένα για ανάγνωση , αρα ξυπνα την read

	Mutex_Unlock(& pipe_ctrl->mut);
	preempt_off; 
	return count; // Returns the number of bytes/chars it wrote  
}

int pipe_reader_close(void* ctrl_block) {

	pipe_ctrl_block *pipe_ctrl = (pipe_ctrl_block*) ctrl_block;
	pipe_ctrl->reader = NULL ;
	Cond_Broadcast (&(pipe_ctrl->space_var)) ;
	if (pipe_ctrl->writer== NULL ) free (pipe_ctrl) ;
	return 0; 
}

int pipe_writer_close(void* ctrl_block) {

	pipe_ctrl_block *pipe_ctrl = (pipe_ctrl_block*) ctrl_block;
	pipe_ctrl->writer = NULL ;	
	if (pipe_ctrl->reader== NULL ) free (pipe_ctrl) ;
	Cond_Broadcast (&(pipe_ctrl->data_var)) ;
	return 0; 
}

file_ops pipeReader_fops = {
  .Open = NULL,
  .Read = pipe_read,
  .Write = dummy,
  .Close = pipe_reader_close
};


file_ops pipeWrite_fops = {
  .Open = NULL,
  .Read = dummy,
  .Write = pipe_write,
  .Close = pipe_writer_close
};

int Pipe(pipe_t* pipe) {
	
	FCB* fcb_reader;
	FCB* fcb_writer;
	
	Mutex_Lock(&kernel_mutex);

	if((!FCB_reserve(1, &pipe->read, &fcb_reader)) | (!FCB_reserve(1, &pipe->write, &fcb_writer))) {// If the fids are exhausted 
		Mutex_Unlock(&kernel_mutex);
		return -1;
	}
	else {
		// Initialize a pipe control block and link it to streamobj (of both fids)
		pipe_ctrl_block* newPipe = (pipe_ctrl_block*) xmalloc(sizeof(pipe_ctrl_block));

		fcb_reader->streamobj = newPipe ;
		fcb_writer->streamobj = newPipe ;

		// Initialize a pipeWrite_fops and pipeRead_fops and link them to the respective streamfuncs

	    fcb_reader->streamfunc = &pipeReader_fops ;
	    fcb_writer->streamfunc = &pipeWrite_fops ;

	    newPipe->reader = fcb_reader; 
	    newPipe->writer = fcb_writer ; 

	    newPipe->data_var = COND_INIT; 
	    newPipe->space_var = COND_INIT; 

	    newPipe->mut = MUTEX_INIT ; 

	    newPipe->head = 0;
	    newPipe->numOfElements = 0 ; 
	   
	    Mutex_Unlock(&kernel_mutex);	

	    return 0 ; 

	}

   

}
