//============================================================================
//
//% Student Name 1: Tal Kazakov
//% Student 1 #:301319792
//% Student 1 userid (email): tal_kazakovsfu.ca
//
//% Student Name 2: Rameshwar Kannan
//% Student 2 #: 301300734
//% Student 2 userid (email): rkannan@sfu.ca
//
//% Below, edit to list any people who helped you with the code in this file,
//%      or put 'None' if nobody helped (the two of) you.
//
// Helpers: _everybody helped us/me with the assignment (list names or put 'None')__
//
// Also, list any resources beyond the course textbooks and the course pages on Piazza
// that you used in making your submission.
//
// Resources:  Karan Nedungadi, TAs, Craig
//
//%% Instructions:
//% * Put your name(s), student number(s), userid(s) in the above section.
//% * Also enter the above information in other files to submit.
//% * Edit the "Helpers" line and, if necessary, the "Resources" line.
//% * Your group name should be "P1_<userid1>_<userid2>" (eg. P1_stu1_stu2)
//% * Form groups as described at:  https://courses.cs.sfu.ca/docs/students
//% * Submit files to courses.cs.sfu.ca
//
// File Name   : myIO.cc
// Version     : September 3rd, 2019
// Description : Starting point for ENSC 351 Project
// Original portions Copyright (c) 2019 Craig Scratchley  (wcs AT sfu DOT ca)
//============================================================================

#include <unistd.h>			// for read/write/close
#include <fcntl.h>			// for open/creat
#include <sys/socket.h> 		// for socketpair
#include "SocketReadcond.h"
#include <mutex>
#include <condition_variable>
#include <vector>
#include <iostream>

class myVariable
{
public:
    std::mutex m;
    std::condition_variable cv;
    std::condition_variable cv1;
    int buffered_data;
    int pairnum;

    myVariable()
    {
    	pairnum = -1;
    	buffered_data = 0;
    }
};

std::vector<myVariable*> myVector;
std::mutex mut;


int mySocketpair( int domain, int type, int protocol, int des[2] )
{
    int max_des;
	int returnVal = socketpair(domain, type, protocol, des);

    //Find the largest descriptor
	if(des[0] > des[1])
	{
	    max_des = des[0];
	} else {

	    max_des = des[1];
	}

	//Make the vector large enough to hold the largest descriptor
    if(myVector.size() <= max_des)
    {
	myVector.resize(max_des+1);
    }

	myVector[des[0]] = new myVariable;
	myVector[des[1]] = new myVariable;

	//Assign a pair number for each socket
	myVector[des[0]]->pairnum = des[1];
	myVector[des[1]]->pairnum = des[0];

	return returnVal;
}

int myOpen(const char *pathname, int flags, mode_t mode)
{
	std::lock_guard<std::mutex> lock(mut);

    int des = open(pathname, flags, mode);

    //Make the vector large enough to hold the largest descriptor
    if(myVector.size() <= des)
    {
      myVector.resize(des+1);
    }

    myVector[des] = new myVariable;

    return des;

}

int myCreat(const char *pathname, mode_t mode)
{

	std::lock_guard<std::mutex> lock(mut);

	int des = creat(pathname, mode);

	//Make the vector large enough to hold the largest descriptor
    if(myVector.size() <= des)
    {
      myVector.resize(des+1);
    }

    myVector[des] = new myVariable;

    return des;
}

ssize_t myWrite( int fildes, const void* buf, size_t nbyte )
{
    int bytesWrite;

    std::unique_lock<std::mutex> lock(myVector[fildes]->m);

    bytesWrite = write(fildes, buf, nbyte );

    //Increment the buffer based on the number of bytes written
    myVector[fildes]->buffered_data += bytesWrite;

    //ALways notify when data gets written to the buffer
    myVector[fildes]->cv1.notify_all();



	return bytesWrite;

    //}
}

int myClose( int fd )
{

	int des = close(fd);
	int pairnum;
	std::unique_lock<std::mutex> lock(mut);

	pairnum = myVector[fd]->pairnum;


    if(pairnum != -1)
    {

    //Before closing, make sure that no thread gets stuck in a wait condition
    //Clear all the wait
	if(myVector[pairnum]->buffered_data > 0)
	{
		myVector[pairnum]->buffered_data = 0;
		myVector[pairnum]->cv.notify_all();

	} else if (myVector[pairnum]->buffered_data < 0) {

		myVector[pairnum]->buffered_data = 0;
		myVector[pairnum]->cv1.notify_all();
	}


	//For the current socket, make sure that nothing gets stuck in the wait condition
	if(myVector[fd]->buffered_data > 0)
    {
		   myVector[fd]->buffered_data = 0;
			myVector[fd]->cv.notify_all();

    } else if (myVector[fd]->buffered_data < 0) {

    	  myVector[fd]->buffered_data = 0;
		  myVector[fd]->cv1.notify_all();

    }

    }

    //Delete the allocated memory
	if(!(myVector[fd]-> pairnum == -1))
	{

		delete myVector[fd];
		myVector[fd] = NULL;
		return des;
	}
	else
	{
		delete myVector[des];
		myVector[des]= NULL;
		return des;

	}
	//return close(fd);
}

int myTcdrain(int des)
{ //is also included for purposes of the course.

    //Lock the mutex
    std::unique_lock<std::mutex> lock(myVector[des]->m);

    //Wait for the buffered data to drain or go below 0
    myVector[des]->cv.wait(lock,[des] {return myVector[des]->buffered_data <= 0;});

	return 0;
}

int myReadcond(int des, void * buf, int n, int min, int time, int timeout)
{
    int bytesRead;
    int pairnum;

    std::unique_lock<std::mutex> lock(myVector[des]->m);

    pairnum = myVector[des]->pairnum;

    //If the buffered data is less than minimum, notify tcdrain and wait until the buffer goes above threshold
    if( myVector[pairnum]->buffered_data < min)
    {
    	myVector[pairnum]->buffered_data -= min;

    	myVector[pairnum]->cv.notify_all();

    	myVector[pairnum]->cv1.wait(lock,[pairnum,min] {return myVector[pairnum]->buffered_data >= 0;});

    	bytesRead = wcsReadcond(des, buf, n, min, time, timeout );

    	myVector[pairnum]->buffered_data -= bytesRead;

    	myVector[pairnum]->buffered_data += min;


    	if(myVector[pairnum]->buffered_data <= 0) // or ==
    	{
    	        //std::cout << "notify " << des;
    	        myVector[pairnum]->cv.notify_all();
    	}

    	return bytesRead;

    } else { //If buffered data is greater than min, read the data and notify Tcdrain

          bytesRead = wcsReadcond(des, buf, n, min, time, timeout );

          myVector[pairnum]->buffered_data -= bytesRead;

          if(myVector[pairnum]->buffered_data <= 0) // or ==
          {
        //std::cout << "notify " << des;
              myVector[pairnum]->cv.notify_all();
          }

	       return bytesRead;


    }
}

ssize_t myRead( int fildes, void* buf, size_t nbyte )
{

	//Only if it is a socket, call myreadCond

    if (myVector[fildes]->pairnum <= -1) // or ==
    {
        return read(fildes, buf, nbyte);

    } else {

        return myReadcond(fildes, buf, nbyte, 1, 0, 0 );

    }


}
