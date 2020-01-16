//============================================================================
//
//% Student Name 1: Tal Kazakov
//% Student 1 #: 301319792
//% Student 1 userid (email): tal_kazakov@sfu.ca (stu1@sfu.ca)
//
//% Student Name 2: Rameshwar Kannan
//% Student 2 #: 301300734
//% Student 2 userid (email): rkannan@sfu.ca (stu2@sfu.ca)
//
//% Below, edit to list any people who helped you with the code in this file,
//%      or put 'None' if nobody helped (the two of) you.
//
// Helpers: TAs
//
// Also, list any resources beyond the course textbooks and the course pages on Piazza
// that you used in making your submission.
//
// Resources:  ___________
//
//%% Instructions:
//% * Put your name(s), student number(s), userid(s) in the above section.
//% * Also enter the above information in other files to submit.
//% * Edit the "Helpers" line and, if necessary, the "Resources" line.
//% * Your group name should be "P1_<userid1>_<userid2>" (eg. P1_stu1_stu2)
//% * Form groups as described at:  https://courses.cs.sfu.ca/docs/students
//% * Submit files to courses.cs.sfu.ca
//
// File Name   : SenderX.cc
// Version     : September 3rd, 2019
// Description : Starting point for ENSC 351 Project
// Original portions Copyright (c) 2019 Craig Scratchley  (wcs AT sfu DOT ca)
//============================================================================

#include <iostream>
#include <stdint.h> // for uint8_t
#include <string.h> // for memset()
#include <errno.h>
#include <fcntl.h>  // for O_RDWR

#include "myIO.h"
#include "SenderX.h"

using namespace std;

SenderX::
SenderX(const char *fname, int d)
:PeerX(d, fname), bytesRd(-1), blkNum(255)
{
}

//-----------------------------------------------------------------------------

/* tries to generate a block.  Updates the
variable bytesRd with the number of bytes that were read
from the input file in order to create the block. Sets
bytesRd to 0 and does not actually generate a block if the end
of the input file had been reached when the previously generated block was
prepared or if the input file is empty (i.e. has 0 length).
*/
void SenderX::genBlk(blkT blkBuf)
{
    // ********* The next line needs to be changed ***********
    if (-1 == (bytesRd = myRead(transferringFileD, &blkBuf[3], CHUNK_SZ )))
        ErrorPrinter("myRead(transferringFileD, &blkBuf[3], CHUNK_SZ )", __FILE__, __LINE__, errno);
    // ********* and additional code must be written ***********
    else {
          //add first three parts of the block
          blkBuf[0] = SOH;
          blkBuf[1] = blkNum;
          blkBuf[2] = 255-blkNum;
       //if bytesrd is less than 128, pad it with ctrlz
       if(bytesRd < CHUNK_SZ)
       {
           for(int i = bytesRd; i<128; i++)
           {
               blkBuf[i+3] = CTRL_Z;
           }
       }

    uint8_t checksum = 0;

    //Check if in crc mode
    if (Crcflg)
    {
        //Craig said the shifting should occur within Crc Fn. However, we did this instead.
        uint16_t myCrc16ns;
        this->crc16ns(&myCrc16ns, &blkBuf[3]);
        blkBuf[131] = (myCrc16ns >> 8);
        blkBuf[132] = myCrc16ns;


    } else { //do checksum

        //Followed the checksum procedure provided in xmodem_edited.txt
        for(int j=0; j < CHUNK_SZ; j++)
        {
           checksum = checksum + blkBuf[j+3];
        }
       while (checksum > 255)
       {
             checksum = checksum - 256;
       }

       blkBuf[CHUNK_SZ+3] = checksum;

    }


   }

    // ********* The next couple lines need to be changed ***********
    //uint16_t myCrc16ns;
    //this->crc16ns(&myCrc16ns, &blkBuf[0]);
}

void SenderX::sendFile()
{
    transferringFileD = myOpen(fileName, O_RDWR, 0);
    if(transferringFileD == -1) {
        // ********* fill in some code here to write 2 CAN characters ***********
        cout /* cerr */ << "Error opening input file named: " << fileName << endl;
        result = "OpenError";
        sendByte(CAN);
        sendByte(CAN);
    }
    else {
        cout << "Sender will send " << fileName << endl;

        // ********* re-initialize blkNum as you like ***********
        blkNum = 1; // but first block sent will be block #1, not #0

        // do the protocol, and simulate a receiver that positively acknowledges every
        //  block that it receives.

        // assume 'C' or NAK received from receiver to enable sending with CRC or checksum, respectively
        genBlk(blkBuf); // prepare 1st block
        while (bytesRd)
        {

            if (blkNum > 255)
            {
                blkNum = 0;
            }


            // ********* fill in some code here to write a block ***********
            if (Crcflg == 1)
            {
                int retVal = myWrite( mediumD, &blkBuf, 133 );
                //Handle the return value of mywrite function
                if(retVal == -1)
                    ErrorPrinter("myWrite(mediumD, &blkBuf, sizeof(blkBuf))", __FILE__, __LINE__, errno);
            } else {
                int retVal =myWrite( mediumD, &blkBuf, 132 );
                //Handle the return value of my write function
                if(retVal == -1)
                    ErrorPrinter("myWrite(mediumD, &blkBuf, sizeof(blkBuf))", __FILE__, __LINE__, errno);
            }

            // assume sent block will be ACK'd
            blkNum ++; // 1st block about to be sent or previous block was ACK'd
            genBlk(blkBuf); // prepare next block
            // assume sent block was ACK'd
        };
        // finish up the protocol, assuming the receiver behaves normally
        // ********* fill in some code here ***********

        sendByte(EOT);
        sendByte(EOT);
        //(myClose(transferringFileD));
        if (-1 == myClose(transferringFileD))
            ErrorPrinter("myClose(transferringFileD)", __FILE__, __LINE__, errno);
        result = "Done";
    }
}

