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
// Helpers: _everybody helped us/me with the assignment (list names or put 'None')__TAs
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
//% * Your group name should be "P2_<userid1>_<userid2>" (eg. P2_stu1_stu2)
//% * Form groups as described at:  https://courses.cs.sfu.ca/docs/students
//% * Submit files to courses.cs.sfu.ca
//
// File Name   : ReceiverX.cpp
// Version     : September 3rd, 2019
// Description : Starting point for ENSC 351 Project Part 2
// Original portions Copyright (c) 2019 Craig Scratchley  (wcs AT sfu DOT ca)
//============================================================================

#include <string.h> // for memset()
#include <fcntl.h>
#include <stdint.h>
#include <iostream>
#include "myIO.h"
#include "ReceiverX.h"
#include "VNPE.h"

//using namespace std;

ReceiverX::
ReceiverX(int d, const char *fname, bool useCrc)
:PeerX(d, fname, useCrc), 
NCGbyte(useCrc ? 'C' : NAK),
goodBlk(false), 
goodBlk1st(false), 
syncLoss(false),
numLastGoodBlk(1)
{
}


void ReceiverX::receiveFile()
{
    mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
        transferringFileD = PE2(myCreat(fileName, mode), fileName);

        uint8_t blknum = 1;
        uint8_t errcnt = 0;


        result = "Cancelled";

        //Start the transfer

        sendByte(NCGbyte);
        if(NCGbyte == 'C')
        {
            Crcflg = true;

        } else {

            Crcflg = false;
        }


        //Read the received data and check if the character is SOH
        //If SOH, validate and get the rest of the block
        while(PE_NOT(myRead(mediumD, rcvBlk, 1), 1), (rcvBlk[0] == SOH))
        {

           //Get the rest of the block. Sets the goodBlk and goodBlk1st to true if it is a good block
            getRestBlk();

            //If goodBlk, ACK it and write it to the chunk
            if(goodBlk)
            {
                sendByte(ACK); // assume the expected block was received correctly.
                errcnt = 0;

                if(rcvBlk[1] == (blknum) && goodBlk1st){

                 writeChunk();
                 blknum++;
                }

            //If it is previous block, ACK it
            } else if (rcvBlk[1] == (blknum - 1)) {

                errcnt = 0;
                sendByte(ACK);

            //If there is syncloss, end the transfer
            } else if (syncLoss) {

                can8();
                result = "Syncloss";
                break;

            //Else NAK it and increment the error count
            } else {

                if(errcnt >= errB)
                {
                    can8();
                    result = "Cancelled";
                    break;
                }
                sendByte(NAK);
                errcnt++;
                //break;

            }
        };
        // check if an EOT is received
        //If so, read another EOT and end the transfer
        if(rcvBlk[0] == EOT)
        {
           sendByte(NAK); // NAK the first EOT
           PE_NOT(myRead(mediumD, rcvBlk, 1), 1); // presumably read in another EOT
           if(rcvBlk[0] == EOT)
           {
                sendByte(ACK); // ACK the second EOT
                result = "Done";

           } else {
                result = "UnexpectedC";
             }
        }

        //---Check if a CAN is received from the sender
        if(rcvBlk[0] == CAN)
        {
            PE_NOT(myRead(mediumD, rcvBlk, 1), 1);
            if(rcvBlk[0] == CAN)
            {
                result = "Cancelled";
            } else {

                result = "UnexpectedC";
            }
        }





        (close(transferringFileD));
        // check if the file closed properly.  If not, result should be something other than "Done".
        //result = "Done"; //assume the file closed properly.
        //sendByte(ACK); // ACK the second EOT
}

/* Only called after an SOH character has been received.
The function tries
to receive the remaining characters to form a complete
block.  The member
variable goodBlk1st will be made true if this is the first
time that the block was received in "good" condition.
*/
void ReceiverX::getRestBlk()
{
    // Check if it is CRC or checksum and read the appropriate data
    if(Crcflg)
    {
        PE_NOT(myReadcond(mediumD, &rcvBlk[1], REST_BLK_SZ_CRC, REST_BLK_SZ_CRC, 0, 0), REST_BLK_SZ_CRC);
    } else {
        PE_NOT(myReadcond(mediumD, &rcvBlk[1], REST_BLK_SZ_CS, REST_BLK_SZ_CS, 0, 0), REST_BLK_SZ_CS);
    }


    uint8_t checksum = 0;
    uint8_t crctemp;

    //Check if the block number is good
    if(numLastGoodBlk == rcvBlk[1])
      {
        //Check if the block number and its complement add up to 255
        if(rcvBlk[1] + rcvBlk[2] == 255)
        {

               //If crcflg, calculate the value and compare it
                if(Crcflg) // for crcmode check
                {
                    uint16_t myreceiveCrc16ns;
                    crc16ns(&myreceiveCrc16ns, &rcvBlk[3]);
                    crctemp = myreceiveCrc16ns;

                    //check if crc values matches with myreceiveCrc16ns, if not send NAK
                    if(rcvBlk[132] == (myreceiveCrc16ns >> 8) && rcvBlk[131] == crctemp)
                    {
                        //sendByte(ACK);
                        //writeChunk();

                        numLastGoodBlk++;


                    } else { // CRC Failed. The blocks must be FALSE
                        goodBlk1st = goodBlk = false;
                        return;
                    }

                } else { //if checksum, calculate and compare

                    for(int j=0; j < CHUNK_SZ; j++) // loop to calculate checksum
                    {
                        checksum = checksum + rcvBlk[j+3];
                    }
                    while (checksum > 255)
                    {
                        checksum = checksum - 256;
                    }
                    if(checksum == rcvBlk[131])
                    {
                        //sendByte(ACK);
                        //writeChunk();

                        numLastGoodBlk++;

                    } else { // CheckSum Failed. The blocks must be FALSE

                        goodBlk1st = goodBlk = false;
                        return;
                    }
                }
              } else {

                  goodBlk1st = goodBlk = false;
                  return;

              }

            } else { //If the block number is false, check for sync loss.

                goodBlk1st = goodBlk = false;
                if(rcvBlk[1] != numLastGoodBlk-1)
                {
                    syncLoss = true;
                }
                return;

            }
    // Everything is validated and goodBlk will be set to true
    goodBlk1st = goodBlk = true;
}

//Write chunk (data) in a received block to disk
void ReceiverX::writeChunk()
{
    PE_NOT(write(transferringFileD, &rcvBlk[DATA_POS], CHUNK_SZ), CHUNK_SZ);
}

//Send 8 CAN characters in a row to the XMODEM sender, to inform it of
//  the cancelling of a file transfer
void ReceiverX::can8()
{
    // no need to space CAN chars coming from receiver in time
    char buffer[CAN_LEN];
    memset( buffer, CAN, CAN_LEN);
    PE_NOT(myWrite(mediumD, buffer, CAN_LEN), CAN_LEN);
}
