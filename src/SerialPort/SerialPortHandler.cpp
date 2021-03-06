/**
 * \file SerialPortHandler.cpp
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

#include "SerialPortHandler.h"
#include <boost/system/system_error.hpp>
#include "HdlcdServerHandler.h"
#include "SerialPortHandlerCollection.h"
#include "ProtocolState.h"
#include <string.h>

SerialPortHandler::SerialPortHandler(const std::string &a_SerialPortName, std::shared_ptr<SerialPortHandlerCollection> a_SerialPortHandlerCollection, boost::asio::io_service &a_IOService): m_SerialPort(a_IOService), m_IOService(a_IOService) {
    m_Registered = true;
    m_SerialPortName = a_SerialPortName;
    m_SerialPortHandlerCollection = a_SerialPortHandlerCollection;
    m_SendBufferOffset = 0;
    ::memset(m_BufferTypeSubscribers, 0x00, sizeof(m_BufferTypeSubscribers));
}

SerialPortHandler::~SerialPortHandler() {
    Stop();
}

void SerialPortHandler::AddHdlcdServerHandler(std::shared_ptr<HdlcdServerHandler> a_HdlcdServerHandler) {
    assert(a_HdlcdServerHandler->GetBufferType() < BUFFER_TYPE_ARITHMETIC_ENDMARKER);
    ++(m_BufferTypeSubscribers[a_HdlcdServerHandler->GetBufferType()]);
    m_HdlcdServerHandlerList.push_back(a_HdlcdServerHandler);
    if ((m_ProtocolState) && (m_ProtocolState->IsRunning())) {
        // Trigger state update messages, to inform the freshly added client
        PropagateSerialPortState();
    } // if
}

void SerialPortHandler::SuspendSerialPort() {
    if (m_Registered == false) {
        return;
    } // if

    if (m_SerialPortLock.SuspendSerialPort()) {
        // The serial port is now suspended!
        m_ProtocolState->Stop();
        m_SerialPort.cancel();
        m_SerialPort.close();
    } // if
    
    PropagateSerialPortState();
}

void SerialPortHandler::ResumeSerialPort() {
    if (m_Registered == false) {
        return;
    } // if

    if (m_SerialPortLock.ResumeSerialPort()) {
        // The serial port is now resumed. Restart activity.
        OpenSerialPort();
    } else {
        PropagateSerialPortState();
    } // else
}

void SerialPortHandler::PropagateSerialPortState() {
    ForEachHdlcdServerHandler([this](std::shared_ptr<HdlcdServerHandler> a_HdlcdServerHandler) {
        a_HdlcdServerHandler->UpdateSerialPortState(m_ProtocolState->IsAlive(), m_SerialPortLock.GetLockHolders());
    });
}

void SerialPortHandler::DeliverPayloadToHDLC(const std::vector<unsigned char> &a_Payload, bool a_bReliable) {
    m_ProtocolState->SendPayload(a_Payload, a_bReliable);
}

bool SerialPortHandler::RequiresBufferType(E_BUFFER_TYPE a_eBufferType) const {
    assert(a_eBufferType < BUFFER_TYPE_ARITHMETIC_ENDMARKER);
    return (m_BufferTypeSubscribers[a_eBufferType] != 0);
}

void SerialPortHandler::DeliverBufferToClients(E_BUFFER_TYPE a_eBufferType, const std::vector<unsigned char> &a_Payload, bool a_bReliable, bool a_bInvalid, bool a_bWasSent) {
    ForEachHdlcdServerHandler([a_eBufferType, &a_Payload, a_bReliable, a_bInvalid, a_bWasSent](std::shared_ptr<HdlcdServerHandler> a_HdlcdServerHandler) {
        a_HdlcdServerHandler->DeliverBufferToClient(a_eBufferType, a_Payload, a_bReliable, a_bInvalid, a_bWasSent);
    });
}

bool SerialPortHandler::Start() {
    m_ProtocolState = std::make_shared<ProtocolState>(shared_from_this(), m_IOService);
    return OpenSerialPort();
}

void SerialPortHandler::Stop() {
    if (m_Registered) {
        m_Registered = false;
        
        // Keep a copy here to keep this object alive!
        auto self(shared_from_this());
        m_SerialPort.cancel();
        m_SerialPort.close();
        m_ProtocolState->Shutdown();
        if (auto l_SerialPortHandlerCollection = m_SerialPortHandlerCollection.lock()) {
            l_SerialPortHandlerCollection->DeregisterSerialPortHandler(self);
        } // if
        
        ForEachHdlcdServerHandler([](std::shared_ptr<HdlcdServerHandler> a_HdlcdServerHandler) {
            a_HdlcdServerHandler->Stop();
        });
    } // if
}

bool SerialPortHandler::OpenSerialPort() {
    bool l_bResult = true;
    try {
        // Open the serial port and start processing
        m_SerialPort.open(m_SerialPortName);
        m_SerialPort.set_option(boost::asio::serial_port::parity(boost::asio::serial_port::parity::none));
        m_SerialPort.set_option(boost::asio::serial_port::character_size(boost::asio::serial_port::character_size(8)));
        m_SerialPort.set_option(boost::asio::serial_port::stop_bits(boost::asio::serial_port::stop_bits::one));
        m_SerialPort.set_option(boost::asio::serial_port::flow_control(boost::asio::serial_port::flow_control::none));
        m_SerialPort.set_option(boost::asio::serial_port::baud_rate(m_BaudRate.GetBaudRate()));
        
        // Start processing
        m_ProtocolState->Start();
        DoRead();
        
        // Trigger first state update message
        PropagateSerialPortState();
    } catch (boost::system::system_error& error) {
        std::cerr << error.what() << std::endl;
        l_bResult = false;
        m_Registered = false;
        
        // TODO: ugly, code duplication. We must assure that cancel is not called!
        auto self(shared_from_this());
        m_ProtocolState->Shutdown();
        if (auto l_SerialPortHandlerCollection = m_SerialPortHandlerCollection.lock()) {
            l_SerialPortHandlerCollection->DeregisterSerialPortHandler(self);
        } // if
        
        ForEachHdlcdServerHandler([](std::shared_ptr<HdlcdServerHandler> a_HdlcdServerHandler) {
            a_HdlcdServerHandler->Stop();
        });
    } // catch

    return l_bResult;
}

void SerialPortHandler::ChangeBaudRate() {
    if (m_Registered) {
        m_BaudRate.ToggleBaudRate();
        m_SerialPort.set_option(boost::asio::serial_port::baud_rate(m_BaudRate.GetBaudRate()));
    } // if
}

void SerialPortHandler::TransmitHDLCFrame(const std::vector<unsigned char> &a_Payload) {
    // Copy buffer holding the escaped HDLC frame for transmission via the serial interface
    assert(m_SendBufferOffset == 0);
    assert(m_SerialPortLock.GetSerialPortState() == false);
    m_SendBuffer = std::move(a_Payload);
    
    // Trigger transmission
    DoWrite();
}

void SerialPortHandler::QueryForPayload(bool a_bQueryReliable, bool a_bQueryUnreliable) {
    ForEachHdlcdServerHandler([a_bQueryReliable, a_bQueryUnreliable](std::shared_ptr<HdlcdServerHandler> a_HdlcdServerHandler) {
        a_HdlcdServerHandler->QueryForPayload(a_bQueryReliable, a_bQueryUnreliable);
    });
}

void SerialPortHandler::DoRead() {
    auto self(shared_from_this());
    m_SerialPort.async_read_some(boost::asio::buffer(m_ReadBuffer, max_length),[this, self](boost::system::error_code a_ErrorCode, std::size_t a_BytesRead) {
        if (!a_ErrorCode) {
            m_ProtocolState->AddReceivedRawBytes(m_ReadBuffer, a_BytesRead);
            if (m_SerialPortLock.GetSerialPortState() == false) {
                DoRead();
            } // if
        } else {
            if (m_SerialPortLock.GetSerialPortState() == false) {
                std::cerr << "SERIAL READ ERROR:" << a_ErrorCode << std::endl;
                Stop();
            } // if
        } 
    });
}

void SerialPortHandler::DoWrite() {
    auto self(shared_from_this());
    m_SerialPort.async_write_some(boost::asio::buffer(&m_SendBuffer[m_SendBufferOffset], (m_SendBuffer.size() - m_SendBufferOffset)),[this, self](boost::system::error_code a_ErrorCode, std::size_t a_BytesSent) {
        if (!a_ErrorCode) {
            m_SendBufferOffset += a_BytesSent;
            if (m_SendBufferOffset == m_SendBuffer.size()) {
                // Indicate that we are ready to transmit the next HDLC frame
                m_SendBufferOffset = 0;
                if (m_SerialPortLock.GetSerialPortState() == false) {
                    m_ProtocolState->TriggerNextHDLCFrame();
                } // if
            } else {
                // Only a partial transmission. We are not done yet.
                if (m_SerialPortLock.GetSerialPortState() == false) {
                    DoWrite();
                } // if
            } // else
        } else {
            if (m_SerialPortLock.GetSerialPortState() == false) {
                std::cerr << "SERIAL WRITE ERROR:" << a_ErrorCode << std::endl;
                Stop();
            } // if
        } // else
    });
}

void SerialPortHandler::ForEachHdlcdServerHandler(std::function<void(std::shared_ptr<HdlcdServerHandler>)> a_Function) {
    assert(a_Function);
    bool l_RebuildSubscriptions = false;
    static bool s_bCyclicCallGuard = false;
    for (auto cur = m_HdlcdServerHandlerList.begin(); cur != m_HdlcdServerHandlerList.end();) {
        auto next = cur;
        ++next;
        if (auto l_ClientHandler = cur->lock()) {
            // Be careful here, as there are cyclic calls back to this method resulting in an invalid "next" iterator!
            bool l_bGuardLocked = false;
            if (!s_bCyclicCallGuard) {
                s_bCyclicCallGuard = true;
                l_bGuardLocked = true;
            } // if

            a_Function(l_ClientHandler);
            if (l_bGuardLocked) {
                s_bCyclicCallGuard = false;
            } // if
        } else {
            // Outdated entry. Only remove it if this is not a cyclic call
            if (!s_bCyclicCallGuard) {
                m_HdlcdServerHandlerList.erase(cur);
                l_RebuildSubscriptions = true;
            } // if
        } // else
        
        cur = next;
    } // for
    
    if (l_RebuildSubscriptions) {
        // Rebuild the subscription database
        ::memset(m_BufferTypeSubscribers, 0x00, sizeof(m_BufferTypeSubscribers));
        ForEachHdlcdServerHandler([this](std::shared_ptr<HdlcdServerHandler> a_HdlcdServerHandler) {
            assert(a_HdlcdServerHandler->GetBufferType() < BUFFER_TYPE_ARITHMETIC_ENDMARKER);
            ++(m_BufferTypeSubscribers[a_HdlcdServerHandler->GetBufferType()]);
        });
    } // if
}
