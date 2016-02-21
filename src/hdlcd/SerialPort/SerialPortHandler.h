/**
 * \file SerialPortHandler.h
 * \brief 
 *
 * The hdlc-tools implement the HDLC protocol to easily talk to devices connected via serial communications
 * Copyright (C) 2016  Florian Evers, florian-evers@gmx.de
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef SERIAL_PORT_HANDLER_H
#define SERIAL_PORT_HANDLER_H

#include <memory>
#include <string>
#include <vector>
#include <boost/asio.hpp>
#include "SerialPortLock.h"
#include "../HDLC/HDLCBuffer.h"
#include "../../shared/Direction.h"
class SerialPortHandlerCollection;
class ClientHandler;
class ProtocolState;

class SerialPortHandler: public std::enable_shared_from_this<SerialPortHandler> {
public:
    // CTOR and DTOR
    SerialPortHandler(const std::string &a_SerialPortName, std::shared_ptr<SerialPortHandlerCollection> a_SerialPortHandlerCollection, boost::asio::io_service& a_IOService);
    ~SerialPortHandler();
    
    void AddClientHandler(std::shared_ptr<ClientHandler> a_ClientHandler);
    void DeliverPayloadToHDLC(const std::vector<unsigned char> &a_Payload);
    void DeliverBufferToClients(E_HDLCBUFFER a_eHDLCBuffer, E_DIRECTION a_eDirection, const std::vector<unsigned char> &a_Payload, bool a_bValid);
    
    void Start();
    void Stop();
    
    // Suspend / resume serial port
    void SuspendSerialPort();
    void ResumeSerialPortX();
    size_t GetLockHolders() const;
    void PropagateSerialPortState() const;
    
    // Do not use from external, only by the ProtocolState
    void DeliverHDLCFrame(const std::vector<unsigned char> &a_Payload);

private:
    // Internal helpers
    void do_read();
    void do_write();
    
    // Members
    bool m_Registered;
    boost::asio::serial_port m_SerialPort;
    boost::asio::io_service &m_IOService;
    std::shared_ptr<ProtocolState> m_ProtocolState;
    std::string m_SerialPortName;
    std::weak_ptr<SerialPortHandlerCollection> m_SerialPortHandlerCollection;
    std::vector<std::weak_ptr<ClientHandler>> m_ClientHandlerVector;
    enum { max_length = 1024 };
    char data_[max_length];
    
    std::vector<unsigned char> m_SendBuffer;
    size_t m_SendBufferOffset;
    SerialPortLock m_SerialPortLock;
};

#endif // SERIAL_PORT_HANDLER_H
