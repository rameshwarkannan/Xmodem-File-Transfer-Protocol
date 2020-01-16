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
// File Name   : SenderX.cpp
// Version     : September 3rd, 2019
// Description : Starting point for ENSC 351 Project Part 2
// Original portions Copyright (c) 2019 Craig Scratchley  (wcs AT sfu DOT ca)
//============================================================================

#include <iostream>
#include <stdint.h> // for uint8_t
#include <string.h> // for memset()
#include <fcntl.h>  // for O_RDONLY

#include "myIO.h"
#include "SenderX.h"
#include "VNPE.h"

using namespace std;

SenderX::SenderX(const char *fname, int d)
:PeerX(d, fname),
 bytesRd(-1),
 firstCrcBlk(true),
 blkNum(0)      // but first block sent will be block #1, not #0
{
}

//-----------------------------------------------------------------------------

// get rid of any characters that may have arrived from the medium.
void SenderX::dumpGlitches()
{
    const int dumpBufSz = 20;
    char buf[dumpBufSz];
    int bytesRead;
    while (dumpBufSz == (bytesRead = PE(myReadcond(mediumD, buf, dumpBufSz, 0, 0, 0))));
}

// Send the block, less the block's last byte, to the receiver.
// Returns the block's last byte.

uint8_t SenderX::sendMostBlk(blkT blkBuf)
//uint8_t SenderX::sendMostBlk(uint8_t blkBuf[BLK_SZ_CRC])
{
    const int mostBlockSize = (this->Crcflg ? BLK_SZ_CRC : BLK_SZ_CS) - 1;
    PE_NOT(myWrite(mediumD, blkBuf, mostBlockSize), mostBlockSize);
    return *(blkBuf + mostBlockSize);
}

// Send the last byte of a block to the receiver
void
SenderX::
sendLastByte(uint8_t lastByte)
{
    PE(myTcdrain(mediumD)); // wait for previous part of block to be completely drained from the descriptor
    dumpGlitches();         // dump any received glitches

    PE_NOT(myWrite(mediumD, &lastByte, sizeof(lastByte)), sizeof(lastByte));
}

/* tries to generate a block.  Updates the
variable bytesRd with the number of bytes that were read
from the input file in order to create the block. Sets
bytesRd to 0 and does not actually generate a block if the end
of the input file had been reached when the previously generated block
was prepared or if the input file is empty (i.e. has 0 length).
*/
void SenderX::genBlk(blkT blkBuf)
//void SenderX::genBlk(uint8_t blkBuf[BLK_SZ_CRC])
{
    //read data and store it directly at the data portion of the buffer
    bytesRd = PE(read(transferringFileD, &blkBuf[DATA_POS], CHUNK_SZ ));
    if (bytesRd>0) {
        blkBuf[0] = SOH; // can be pre-initialized for efficiency
        //block number and its complement
        uint8_t nextBlkNum = blkNum + 1;
        blkBuf[SOH_OH] = nextBlkNum;
        blkBuf[SOH_OH + 1] = ~nextBlkNum;
        if (this->Crcflg) {
            /*add padding*/
            if(bytesRd<CHUNK_SZ)
            {
                //pad ctrl-z for the last block
                uint8_t padSize = CHUNK_SZ - bytesRd;
                memset(blkBuf+DATA_POS+bytesRd, CTRL_Z, padSize);
            }

            /* calculate and add CRC in network byte order */
            crc16ns((uint16_t*)&blkBuf[PAST_CHUNK], &blkBuf[DATA_POS]);
        }
        else {
            //checksum
            blkBuf[PAST_CHUNK] = blkBuf[DATA_POS];
            for( int ii=DATA_POS + 1; ii < DATA_POS+bytesRd; ii++ )
                blkBuf[PAST_CHUNK] += blkBuf[ii];

            //padding
            if( bytesRd < CHUNK_SZ )  { // this line could be deleted
                //pad ctrl-z for the last block
                uint8_t padSize = CHUNK_SZ - bytesRd;
                memset(blkBuf+DATA_POS+bytesRd, CTRL_Z, padSize);
                blkBuf[PAST_CHUNK] += CTRL_Z * padSize;
            }
        }
    }
}

/* tries to prepare the first block.
 * The receiver will send either a 'C' or 'NAK' after recieving the first blk.
 * Then, prep1stBlk() will decide based on the receivers' response whether the
 * first blk should use checkSum (Reciever sent 'NAK') or crc (Reciever sent 'C').
*/
void SenderX::prep1stBlk()
{
    //blkNum = 0;
    errCnt = 0;
    Crcflg = true;
    genBlk(blkBuf);
}

/* refit the 1st block with a checksum
*/
void
SenderX::
cs1stBlk()
{

    //Calculate and refit checksum in the first blk
    uint8_t checksum = 0;
    Crcflg = false;
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

/* while sending the now current block for the first time, prepare the next block if possible.
*/
void SenderX::sendBlkPrepNext()
{

    blkNum ++; // 1st block about to be sent or previous block ACK'd

    //---Make a copy of the block
    if (Crcflg)
    {
        for(int i = 0; i<133; i++)
        {
            blkBufCpy[i] = blkBuf[i];
        }

    } else {
        for(int i = 0; i<132; i++)
        {
            blkBufCpy[i] = blkBuf[i];
        }

    }
    uint8_t lastByte = sendMostBlk(blkBuf);
    genBlk(blkBuf); // prepare next block
    sendLastByte(lastByte);
}

// Resends the block that had been sent previously to the xmodem receiver
void SenderX::resendBlk()
{
    // resend the block including the checksum or crc16
    uint8_t lastByte = sendMostBlk(blkBufCpy);
    sendLastByte(lastByte);

}

//Send CAN_LEN copies of CAN characters in a row (in pairs spaced in time) to the
//  XMODEM receiver, to inform it of the canceling of a file transfer.
//  There should be a total of (canPairs - 1) delays of
//  ((TM_2CHAR + TM_CHAR)/2 * mSECS_PER_UNIT) milliseconds
//  between the pairs of CAN characters.
void SenderX::can8()
{
    //  * You will have to write this simple function *
    // use the C++11/14 standard library to generate the delays
    const int canLength = 2;
    const int canPairs = 4;
    //char memLocation[canLength];
    //memset(memLocation, CAN, canLength);
    sendByte(CAN);
    sendByte(CAN);

    for(int i = 0; i<(canPairs-1); i++)
    {
        usleep((TM_2CHAR + TM_CHAR)/2 * mSECS_PER_UNIT*1000);     //Time Delay
        sendByte(CAN);
        sendByte(CAN);

    }

}

void SenderX::sendFile()
{
    const uint8_t START = 1;
    const uint8_t ACKNAK = 2;
    const uint8_t CAN1 = 3;
    const uint8_t EOT1 = 4;
    const uint8_t EOTEOT = 5;
    uint8_t nextstate = 1;
    const uint8_t end = 6;

    transferringFileD = myOpen(fileName, O_RDONLY, 0);
    if(transferringFileD == -1) {
        can8();
        cout /* cerr */ << "Error opening input file named: " << fileName << endl;
        result = "OpenError";
    }
    else {
        //blkNum = 0; // but first block sent will be block #1, not #0


        char byteToReceive;
        int run = 1;

        Crcflg = true;
        firstCrcBlk = true;
        prep1stBlk(); //---Prepares with crc


        while(run)
        {
        //---Follow the state chart
        PE_NOT(myRead(mediumD, &byteToReceive, 1), 1); // assuming get a 'C'

        switch(nextstate) {

             //Start the transfer
             case START:
                 if ((byteToReceive == NAK || byteToReceive == 'C' ) && bytesRd)
                 {
                     //--if NAK is received, refit the blk with checksum
                     if(byteToReceive == NAK)
                     {
                         Crcflg = false;
                         cs1stBlk();
                         firstCrcBlk = false;
                     }
                     sendBlkPrepNext();
                     nextstate = ACKNAK;
                     //break;

                 } else if ((byteToReceive == NAK || byteToReceive == 'C' ) && !bytesRd)
                 {
                     if(byteToReceive == NAK)
                     {
                         firstCrcBlk = false;

                     }
                     sendByte(EOT);
                     nextstate = EOT1;
                     //break;

                 }
                 break;

             //Follow the procedure in state chart
             case ACKNAK:
                 if((byteToReceive == ACK) && bytesRd)
                 {
                     sendBlkPrepNext();
                     errCnt = 0;
                     firstCrcBlk = false;
                     nextstate = ACKNAK;

                 } else if  ((byteToReceive == NAK || (byteToReceive == 'C' && firstCrcBlk)) && (errCnt < errB))
                 {
                     resendBlk();
                     errCnt++;
                     nextstate = ACKNAK;

                 } else if (byteToReceive == ACK && !bytesRd) {

                     sendByte(EOT);
                     errCnt = 0;
                     firstCrcBlk = false;
                     nextstate = EOT1;

                 } else if(byteToReceive == NAK && errCnt >= errB) {

                     can8();
                     result = "ExcessiveNAKs";
                     run = 0;

                 }
                 else if (byteToReceive == CAN) {

                     nextstate = CAN1;
                 }
                 break;

             //When a cancel is received
             case CAN1:
                 if(byteToReceive == CAN)
                 {
                     result = "RcvCancelled";
                     //nextstate = end;
                     run = 0;
                 }
                 break;

             //When an EOT is received
             case EOT1:
                if(byteToReceive == NAK)
                {
                    sendByte(EOT);
                    nextstate = EOTEOT;

                } else if (byteToReceive == ACK)
                {

                    result = "1st EOT ACK'd";
                    //nextstate = end;
                    run = 0;
                }
                break;

             case EOTEOT:
                 if(byteToReceive == NAK && errCnt < errB)
                 {

                     sendByte(EOT);
                     errCnt++;
                     nextstate = EOTEOT;
                 } else if (byteToReceive == ACK)
                 {

                     result = "Done";
                     //nextstate = end;
                     run = 0;

                 } else if (byteToReceive == 'C') {

                     can8();
                     result = "UnexpectedC";
                     //nextstate = end;
                     run = 0;

                 }
                 break;

        }

        }

        PE(myClose(transferringFileD));


    }
}
