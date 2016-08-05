/*====================================================================
Copyright(c) 2016 Adam Rankin


Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files(the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and / or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
====================================================================*/

#pragma once

// Message type includes
#include "CommandReply.h"
#include "TrackedFrameMessage.h"
#include "TrackedFrameReply.h"

// IGT includes
#include "igtlClientSocket.h"
#include "igtlMessageHeader.h"
#include "igtlMessageFactory.h"

// STD includes
#include <deque>
#include <string>

// Windows includes
#include <ppltasks.h>
#include <concrt.h>
#include <vccorlib.h>

namespace WF = Windows::Foundation;
namespace WFC = WF::Collections;
namespace WFM = WF::Metadata;
namespace WUXC = Windows::UI::Xaml::Controls;
namespace WUXM = Windows::UI::Xaml::Media;

namespace UWPOpenIGTLink
{
  ///
  /// \class IGTLinkClient
  /// \brief This class provides an OpenIGTLink client. It has basic functionality for sending and receiving messages
  ///
  /// \description It connects to an IGTLink v3+ server, sends requests and receives responses.
  ///
  [Windows::Foundation::Metadata::WebHostHiddenAttribute]
  public ref class IGTLinkClient sealed
  {
  public:
    IGTLinkClient();
    virtual ~IGTLinkClient();

    property int ServerPort {int get(); void set( int ); }
    property Platform::String^ ServerHost { Platform::String ^ get(); void set( Platform::String^ ); }
    property int ServerIGTLVersion { int get(); void set( int ); }
    property bool Connected { bool get(); }

    /// If timeoutSec<0 then connection will be attempted multiple times until successfully connected or the timeout elapse
    WF::IAsyncOperation<bool>^ ConnectAsync( double timeoutSec );

    /// Disconnect from the connected server
    void Disconnect();

    /// Retrieve the oldest command reply from the queue of replies and clear it
    bool ParseCommandReply( CommandReply^ reply );

    /// Retrieve the oldest tracked frame reply from the queue of replies and clear it
    [Windows::Foundation::Metadata::DefaultOverloadAttribute]
    bool ParseTrackedFrameReply( TrackedFrameMessageCx^ reply );

    /// Retrieve the oldest tracked frame reply from the queue of replies and clear it
    bool ParseTrackedFrameReply( TrackedFrameMessage^ reply );

  internal:
    /// Send a packed message to the connected server
    bool SendMessage( igtl::MessageBase::Pointer packedMessage );

    /// Threaded function to receive data from the connected server
    static void DataReceiverPump( IGTLinkClient^ self, concurrency::cancellation_token token );

    // Callback functions for when a frame is received
    uint64 RegisterTrackedFrameCallback( std::function<void( igtl::TrackedFrameMessage* )>& function );
    bool UnregisterTrackedFrameCallback( uint64 token );

  protected private:
    /// Thread-safe method that allows child classes to read data from the socket
    int SocketReceive( void* data, int length );

    /// Convert a c-style byte array to a managed image object
    bool FromNativePointer( unsigned char* pData, int width, int height, int numberOfcomponents, WUXM::Imaging::WriteableBitmap^ wbm );

  protected private:
    /// igtl Factory for message sending
    igtl::MessageFactory::Pointer m_igtlMessageFactory;

    concurrency::task<void> m_dataReceiverTask;
    concurrency::cancellation_token_source m_cancellationTokenSource;

    /// Mutex instance for safe data access
    Concurrency::critical_section m_messageListMutex;
    Concurrency::critical_section m_socketMutex;

    /// Socket that is connected to the server
    igtl::ClientSocket::Pointer m_clientSocket;

    // Tracked frame callbacks
    std::map < uint64, std::function<void( igtl::TrackedFrameMessage* )> > m_trackedFrameCallbacks;
    uint64 m_lastUnusedCallbackToken = 0;

    /// List of messages received through the socket, transformed to igtl messages
    std::deque<igtl::MessageBase::Pointer> m_messages;

    /// Stored WriteableBitmap to reduce overhead of memory reallocation unless necessary
    WUXM::Imaging::WriteableBitmap^ m_writeableBitmap;
    std::vector<uint32> m_frameSize;

    /// Server information
    Platform::String^ m_ServerHost;
    int m_ServerPort;
    int m_ServerIGTLVersion;

    static const int CLIENT_SOCKET_TIMEOUT_MSEC;
    static const uint32 MESSAGE_LIST_MAX_SIZE;

  private:
    IGTLinkClient( IGTLinkClient^ ) {}
    void operator=( IGTLinkClient^ ) {}
  };

}