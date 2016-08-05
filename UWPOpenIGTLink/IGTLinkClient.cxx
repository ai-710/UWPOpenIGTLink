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

// Local includes
#include "IGTLinkClient.h"
#include "TrackedFrameMessage.h"

// IGT includes
#include "igtlCommandMessage.h"
#include "igtlCommon.h"
#include "igtlMessageHeader.h"
#include "igtlOSUtil.h"
#include "igtlServerSocket.h"
#include "igtlStatusMessage.h"
#include "igtlTrackingDataMessage.h"

// STD includes
#include <chrono>
#include <regex>

// Windows includes
#include <collection.h>
#include <pplawait.h>
#include <ppltasks.h>
#include <robuffer.h>

namespace WF = Windows::Foundation;
namespace WFC = WF::Collections;
namespace WFM = WF::Metadata;
namespace WSS = Windows::Storage::Streams;
namespace WUXC = Windows::UI::Xaml::Controls;
namespace WUXM = Windows::UI::Xaml::Media;
namespace WDXD = Windows::Data::Xml::Dom;

namespace
{
  inline void ThrowIfFailed( HRESULT hr )
  {
    if ( FAILED( hr ) )
    {
      throw Platform::Exception::CreateException( hr );
    }
  }

  byte* GetPointerToPixelData( WSS::IBuffer^ buffer )
  {
    // Cast to Object^, then to its underlying IInspectable interface.
    Platform::Object^ obj = buffer;
    Microsoft::WRL::ComPtr<IInspectable> insp( reinterpret_cast<IInspectable*>( obj ) );

    // Query the IBufferByteAccess interface.
    Microsoft::WRL::ComPtr<WSS::IBufferByteAccess> bufferByteAccess;
    ThrowIfFailed( insp.As( &bufferByteAccess ) );

    // Retrieve the buffer data.
    byte* pixels = nullptr;
    ThrowIfFailed( bufferByteAccess->Buffer( &pixels ) );
    return pixels;
  }
}

namespace UWPOpenIGTLink
{

  const int IGTLinkClient::CLIENT_SOCKET_TIMEOUT_MSEC = 500;
  const uint32 IGTLinkClient::MESSAGE_LIST_MAX_SIZE = 200;

  //----------------------------------------------------------------------------
  IGTLinkClient::IGTLinkClient()
    : m_igtlMessageFactory( igtl::MessageFactory::New() )
    , m_clientSocket( igtl::ClientSocket::New() )
  {
    m_igtlMessageFactory->AddMessageType( "TRACKEDFRAME", ( igtl::MessageFactory::PointerToMessageBaseNew )&igtl::TrackedFrameMessage::New );
    this->ServerHost = L"127.0.0.1";
    this->ServerPort = 18944;
    this->ServerIGTLVersion = IGTL_HEADER_VERSION_2;

    this->m_frameSize.assign( 3, 0 );
  }

  //----------------------------------------------------------------------------
  IGTLinkClient::~IGTLinkClient()
  {
    this->Disconnect();
    auto disconnectTask = concurrency::create_task( [this]()
    {
      while ( this->Connected )
      {
        Sleep( 33 );
      }
    } );
    disconnectTask.wait();
  }

  //----------------------------------------------------------------------------
  Windows::Foundation::IAsyncOperation<bool>^ IGTLinkClient::ConnectAsync( double timeoutSec )
  {
    this->Disconnect();

    this->m_cancellationTokenSource = concurrency::cancellation_token_source();
    auto token = this->m_cancellationTokenSource.get_token();

    return concurrency::create_async( [this, timeoutSec, token]() -> bool
    {
      auto connectTask = concurrency::create_task( [this, timeoutSec, token]() -> bool
      {
        const int retryDelaySec = 1.0;
        int errorCode = 1;
        auto start = std::chrono::high_resolution_clock::now();
        while ( errorCode != 0 )
        {
          std::wstring wideStr( this->ServerHost->Begin() );
          std::string str( wideStr.begin(), wideStr.end() );
          errorCode = this->m_clientSocket->ConnectToServer( str.c_str(), this->ServerPort );
          std::chrono::duration<double, std::milli> timeDiff = std::chrono::high_resolution_clock::now() - start;
          if ( timeDiff.count() > timeoutSec * 1000 )
          {
            // time is up
            break;
          }
          std::this_thread::sleep_for( std::chrono::seconds( retryDelaySec ) );
        }

        if ( errorCode != 0 )
        {
          return false;
        }

        this->m_clientSocket->SetTimeout( CLIENT_SOCKET_TIMEOUT_MSEC );
        return true;
      } );

      // Wait (inside the async operation) and retrieve the result of connection
      bool result = connectTask.get();

      if( result )
      {
        // We're connected, start the data receiver thread
        concurrency::create_task( [this, token]( void )
        {
          this->DataReceiverPump( this, token );
        } );
      }

      return result;
    } );
  }

  //----------------------------------------------------------------------------
  void IGTLinkClient::Disconnect()
  {
    this->m_cancellationTokenSource.cancel();

    {
      Concurrency::critical_section::scoped_lock lock( this->m_socketMutex );
      this->m_clientSocket->CloseSocket();
    }
  }

  //----------------------------------------------------------------------------
  bool IGTLinkClient::SendMessage( igtl::MessageBase::Pointer packedMessage )
  {
    int success = 0;
    {
      Concurrency::critical_section::scoped_lock lock( m_socketMutex );
      success = this->m_clientSocket->Send( packedMessage->GetBufferPointer(), packedMessage->GetBufferSize() );
    }
    if ( !success )
    {
      std::cerr << "OpenIGTLink client couldn't send message to server." << std::endl;
      return false;
    }
    return true;
  }

  //----------------------------------------------------------------------------
  void IGTLinkClient::DataReceiverPump( IGTLinkClient^ self, concurrency::cancellation_token token )
  {
    while ( !token.is_canceled() )
    {
      auto headerMsg = self->m_igtlMessageFactory->CreateHeaderMessage( IGTL_HEADER_VERSION_1 );

      // Receive generic header from the socket
      int numOfBytesReceived = 0;
      {
        Concurrency::critical_section::scoped_lock lock( self->m_socketMutex );
        if ( !self->m_clientSocket->GetConnected() )
        {
          // We've become disconnected while waiting for the socket, we're done here!
          return;
        }
        numOfBytesReceived = self->m_clientSocket->Receive( headerMsg->GetBufferPointer(), headerMsg->GetBufferSize() );
      }
      if ( numOfBytesReceived == 0 // No message received
           || numOfBytesReceived != headerMsg->GetPackSize() // Received data is not as we expected
         )
      {
        // Failed to receive data, maybe the socket is disconnected
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
        continue;
      }

      int c = headerMsg->Unpack( 1 );
      if ( !( c & igtl::MessageHeader::UNPACK_HEADER ) )
      {
        std::cerr << "Failed to receive reply (invalid header)" << std::endl;
        continue;
      }

      auto bodyMsg = self->m_igtlMessageFactory->CreateReceiveMessage( headerMsg );
      if ( bodyMsg.IsNull() )
      {
        std::cerr << "Unable to create message of type: " << headerMsg->GetMessageType() << std::endl;
        continue;
      }

      // Accept all messages but status messages, they are used as a keep alive mechanism
      if ( typeid( *bodyMsg ) != typeid( igtl::StatusMessage ) )
      {
        bodyMsg->SetMessageHeader( headerMsg );
        bodyMsg->AllocateBuffer();
        {
          Concurrency::critical_section::scoped_lock lock( self->m_socketMutex );
          if ( !self->m_clientSocket->GetConnected() )
          {
            // We've become disconnected while waiting for the socket, we're done here!
            return;
          }
          self->m_clientSocket->Receive( bodyMsg->GetBufferBodyPointer(), bodyMsg->GetBufferBodySize() );
        }

        int c = bodyMsg->Unpack( 1 );
        if ( !( c & igtl::MessageHeader::UNPACK_BODY ) )
        {
          std::cerr << "Failed to receive reply (invalid body)" << std::endl;
          continue;
        }

        {
          // save reply
          Concurrency::critical_section::scoped_lock lock( self->m_messageListMutex );

          self->m_messages.push_back( bodyMsg );
        }

        if ( typeid( *bodyMsg ) != typeid( igtl::TrackedFrameMessage ) )
        {
          for ( auto pair : self->m_trackedFrameCallbacks )
          {
            igtl::TrackedFrameMessage* tfMsg = dynamic_cast<igtl::TrackedFrameMessage*>( bodyMsg.GetPointer() );
            pair.second( tfMsg );
          }
        }
      }
      else
      {
        Concurrency::critical_section::scoped_lock lock( self->m_socketMutex );

        if ( !self->m_clientSocket->GetConnected() )
        {
          // We've become disconnected while waiting for the socket, we're done here!
          return;
        }
        self->m_clientSocket->Skip( headerMsg->GetBodySizeToRead(), 0 );
      }

      if ( self->m_messages.size() > MESSAGE_LIST_MAX_SIZE )
      {
        Concurrency::critical_section::scoped_lock lock( self->m_messageListMutex );

        // erase the front N results
        uint32 toErase = self->m_messages.size() - MESSAGE_LIST_MAX_SIZE;
        self->m_messages.erase( self->m_messages.begin(), self->m_messages.begin() + toErase );
      }
    }

    return;
  }

  //----------------------------------------------------------------------------
  bool IGTLinkClient::ParseCommandReply( CommandReply^ reply )
  {
    reply->Result = false;
    reply->Parameters = ref new Platform::Collections::Map<Platform::String^, Platform::String^>;

    igtl::MessageBase::Pointer message = nullptr;
    {
      // Retrieve the next available command reply
      Concurrency::critical_section::scoped_lock lock( m_messageListMutex );
      for ( auto reply : m_messages )
      {
        if ( typeid( *( reply ) ) == typeid( igtl::RTSCommandMessage ) )
        {
          message = reply;
          m_messages.erase( std::find( m_messages.begin(), m_messages.end(), reply ) );
          break;
        }
      }
    }

    if( message != nullptr )
    {
      igtl::RTSCommandMessage::Pointer rtsCommandMsg = dynamic_cast<igtl::RTSCommandMessage*>( message.GetPointer() );

      // TODO : this whole function will need to know about all of the different TUO command message replies in the future

      std::wstring wideStr( rtsCommandMsg->GetCommandName().begin(), rtsCommandMsg->GetCommandName().end() );
      reply->CommandName = ref new Platform::String( wideStr.c_str() );
      reply->OriginalCommandId = rtsCommandMsg->GetCommandId();

      std::wstring wideContentStr( rtsCommandMsg->GetCommandContent().begin(), rtsCommandMsg->GetCommandContent().end() );
      std::transform( wideContentStr.begin(), wideContentStr.end(), wideContentStr.begin(), ::towlower );
      reply->CommandContent = ref new Platform::String( wideContentStr.c_str() );

      WDXD::XmlDocument^ commandDoc = ref new WDXD::XmlDocument;
      commandDoc->LoadXml( reply->CommandContent );
      reply->Result = commandDoc->GetElementsByTagName( L"Result" )->Item( 0 )->NodeValue == L"true";

      for ( unsigned int i = 0; i < commandDoc->GetElementsByTagName( L"Parameter" )->Size; ++i )
      {
        auto entry = commandDoc->GetElementsByTagName( L"Parameter" )->Item( i );
        Platform::String^ name = dynamic_cast<Platform::String^>( entry->Attributes->GetNamedItem( L"Name" )->NodeValue );
        Platform::String^ value = dynamic_cast<Platform::String^>( entry->Attributes->GetNamedItem( L"Value" )->NodeValue );
        reply->Parameters->Insert( name, value );
      }

      for ( auto pair : rtsCommandMsg->GetMetaData() )
      {
        std::wstring keyWideStr( pair.first.begin(), pair.first.end() );
        std::wstring valueWideStr( pair.second.begin(), pair.second.end() );
        reply->Parameters->Insert( ref new Platform::String( keyWideStr.c_str() ), ref new Platform::String( valueWideStr.c_str() ) );
      }

      reply->Result = true;
      return true;
    }

    return false;
  }

  //----------------------------------------------------------------------------
  bool IGTLinkClient::ParseTrackedFrameReply( TrackedFrameMessageCx^ reply )
  {
    reply->Result = false;
    reply->Parameters = ref new Platform::Collections::Map<Platform::String^, Platform::String^>;
    reply->ImageSource = this->m_writeableBitmap;

    igtl::MessageBase::Pointer message = nullptr;
    {
      // Retrieve the next available tracked frame reply
      Concurrency::critical_section::scoped_lock lock( m_messageListMutex );
      for ( auto replyIter = m_messages.begin(); replyIter != m_messages.end(); ++replyIter )
      {
        if ( typeid( *( *replyIter ) ) == typeid( igtl::TrackedFrameMessage ) )
        {
          message = *replyIter;
          m_messages.erase( replyIter );
          break;
        }
      }
    }

    if ( message != nullptr )
    {
      igtl::TrackedFrameMessage::Pointer trackedFrameMsg = dynamic_cast<igtl::TrackedFrameMessage*>( message.GetPointer() );

      for ( auto pair : trackedFrameMsg->GetMetaData() )
      {
        std::wstring keyWideStr( pair.first.begin(), pair.first.end() );
        std::wstring valueWideStr( pair.second.begin(), pair.second.end() );
        reply->Parameters->Insert( ref new Platform::String( keyWideStr.c_str() ), ref new Platform::String( valueWideStr.c_str() ) );
      }

      for ( auto pair : trackedFrameMsg->GetCustomFrameFields() )
      {
        std::wstring keyWideStr( pair.first.begin(), pair.first.end() );
        std::wstring valueWideStr( pair.second.begin(), pair.second.end() );
        reply->Parameters->Insert( ref new Platform::String( keyWideStr.c_str() ), ref new Platform::String( valueWideStr.c_str() ) );
      }

      if ( trackedFrameMsg->GetFrameSize()[0] != this->m_frameSize[0] ||
           trackedFrameMsg->GetFrameSize()[1] != this->m_frameSize[1] ||
           trackedFrameMsg->GetFrameSize()[2] != this->m_frameSize[2] )
      {
        this->m_frameSize.clear();
        this->m_frameSize.push_back( trackedFrameMsg->GetFrameSize()[0] );
        this->m_frameSize.push_back( trackedFrameMsg->GetFrameSize()[1] );
        this->m_frameSize.push_back( trackedFrameMsg->GetFrameSize()[2] );

        // Reallocate a new image
        this->m_writeableBitmap = ref new WUXM::Imaging::WriteableBitmap( m_frameSize[0], m_frameSize[1] );
      }

      FromNativePointer( trackedFrameMsg->GetImage(),
                         trackedFrameMsg->GetFrameSize()[0],
                         trackedFrameMsg->GetFrameSize()[1],
                         trackedFrameMsg->GetNumberOfComponents(),
                         this->m_writeableBitmap );

      this->m_writeableBitmap->Invalidate();
      reply->Result = true;
      return true;
    }

    return false;
  }

  //----------------------------------------------------------------------------
  bool IGTLinkClient::ParseTrackedFrameReply( TrackedFrameMessage^ message )
  {
    igtl::MessageBase::Pointer igtMessage = nullptr;
    {
      // Retrieve the next available tracked frame reply
      Concurrency::critical_section::scoped_lock lock( m_messageListMutex );
      for ( auto replyIter = m_messages.begin(); replyIter != m_messages.end(); ++replyIter )
      {
        if ( typeid( *( *replyIter ) ) == typeid( igtl::TrackedFrameMessage ) )
        {
          igtMessage = *replyIter;
          m_messages.erase( replyIter );
          break;
        }
      }
    }

    if ( igtMessage != nullptr )
    {
      igtl::TrackedFrameMessage::Pointer trackedFrameMsg = dynamic_cast<igtl::TrackedFrameMessage*>( igtMessage.GetPointer() );

      for ( auto pair : trackedFrameMsg->GetMetaData() )
      {
        std::wstring keyWideStr( pair.first.begin(), pair.first.end() );
        std::wstring valueWideStr( pair.second.begin(), pair.second.end() );
        message->Parameters->Insert( ref new Platform::String( keyWideStr.c_str() ), ref new Platform::String( valueWideStr.c_str() ) );
      }

      for ( auto pair : trackedFrameMsg->GetCustomFrameFields() )
      {
        std::wstring keyWideStr( pair.first.begin(), pair.first.end() );
        std::wstring valueWideStr( pair.second.begin(), pair.second.end() );
        message->Parameters->Insert( ref new Platform::String( keyWideStr.c_str() ), ref new Platform::String( valueWideStr.c_str() ) );
      }

      message->SetImageSize( trackedFrameMsg->GetFrameSize()[0], trackedFrameMsg->GetFrameSize()[1], trackedFrameMsg->GetFrameSize()[2] );
      message->ImageSizeBytes = trackedFrameMsg->GetImageSizeInBytes();
      Platform::ArrayReference<unsigned char> arraywrapper( ( unsigned char* )trackedFrameMsg->GetImage(), trackedFrameMsg->GetImageSizeInBytes() );
      auto ibuffer = Windows::Security::Cryptography::CryptographicBuffer::CreateFromByteArray( arraywrapper );
      message->SetImageData( ibuffer );
      message->NumberOfComponents = trackedFrameMsg->GetNumberOfComponents();

      message->Result = true;
      return true;
    }

    return false;
  }

  //----------------------------------------------------------------------------
  int IGTLinkClient::SocketReceive( void* data, int length )
  {
    Concurrency::critical_section::scoped_lock lock( m_socketMutex );
    return m_clientSocket->Receive( data, length );
  }

  //----------------------------------------------------------------------------
  bool IGTLinkClient::FromNativePointer( unsigned char* pData, int width, int height, int numberOfcomponents, WUXM::Imaging::WriteableBitmap^ wbm )
  {
    byte* pPixels = ::GetPointerToPixelData( wbm->PixelBuffer );

    int i( 0 );
    for ( int y = 0; y < height; y++ )
    {
      for ( int x = 0; x < width; x++ )
      {
        pPixels[( x + y * width ) * 4] = pData[i]; // B
        pPixels[( x + y * width ) * 4 + 1] = pData[i]; // G
        pPixels[( x + y * width ) * 4 + 2] = pData[i]; // R
        pPixels[( x + y * width ) * 4 + 3] = 255; // A

        i++;
      }
    }

    return true;
  }

  //----------------------------------------------------------------------------
  uint64 IGTLinkClient::RegisterTrackedFrameCallback( std::function<void( igtl::TrackedFrameMessage* )>& function )
  {
    m_trackedFrameCallbacks[m_lastUnusedCallbackToken] = function;

    m_lastUnusedCallbackToken++;
    return m_lastUnusedCallbackToken - 1;
  }

  //----------------------------------------------------------------------------
  bool IGTLinkClient::UnregisterTrackedFrameCallback( uint64 token )
  {
    if ( m_trackedFrameCallbacks.find( token ) != m_trackedFrameCallbacks.end() )
    {
      m_trackedFrameCallbacks.erase( m_trackedFrameCallbacks.find( token ) );
      return true;
    }

    return false;
  }

  //----------------------------------------------------------------------------
  int IGTLinkClient::ServerPort::get()
  {
    return this->m_ServerPort;
  }

  //----------------------------------------------------------------------------
  void IGTLinkClient::ServerPort::set( int arg )
  {
    this->m_ServerPort = arg;
  }

  //----------------------------------------------------------------------------
  Platform::String^ IGTLinkClient::ServerHost::get()
  {
    return this->m_ServerHost;
  }

  //----------------------------------------------------------------------------
  void IGTLinkClient::ServerHost::set( Platform::String^ arg )
  {
    this->m_ServerHost = arg;
  }

  //----------------------------------------------------------------------------
  int IGTLinkClient::ServerIGTLVersion::get()
  {
    return this->m_ServerIGTLVersion;
  }

  //----------------------------------------------------------------------------
  void IGTLinkClient::ServerIGTLVersion::set( int arg )
  {
    this->m_ServerIGTLVersion = arg;
  }

  //----------------------------------------------------------------------------
  bool IGTLinkClient::Connected::get()
  {
    return this->m_clientSocket->GetConnected();
  }
}