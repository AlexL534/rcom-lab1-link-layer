// Application layer protocol implementation

#include "application_layer.h"

void applicationLayer(const char *serialPort, const char *role, int baudRate, int nTries, int timeout, const char *filename)
{
    LinkLayer linklayer;
    strcpy(linklayer.serialPort, serialPort);
    linklayer.baudRate = baudRate;
    linklayer.nRetransmissions = nTries;
    linklayer.timeout = timeout;
    linklayer.role = strcmp(role, "tx") ? LlRx : LlTx;

    llopen(linklayer);
}