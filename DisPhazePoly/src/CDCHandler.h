#include "initialisation.h"
#include "USB.h"
#include <string>
//#include <sstream>

#define CDC_CMD_LEN 20

extern USB usb;
extern bool CmdPending;
extern char ComCmd[CDC_CMD_LEN];

bool CDCCommand(const std::string ComCmd);
void CDCHandler(const uint8_t* data, uint32_t length);
