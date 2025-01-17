﻿/*====================================================================
Copyright(c) 2018 Adam Rankin


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
#include "pch.h"
#include "IGTLConnectorPage.xaml.h"

// WinRT includes
#include <collection.h>
#include <robuffer.h>

// STL includes
#include <iomanip>
#include <sstream>
#include <string>

using namespace Concurrency;
using namespace UWPOpenIGTLink;
using namespace Windows::Foundation::Collections;
using namespace Windows::Foundation::Numerics;
using namespace Windows::Foundation;
using namespace Windows::Networking;
using namespace Windows::Storage::Streams;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace Windows::UI::Xaml;

namespace
{
  //----------------------------------------------------------------------------
  inline void ThrowIfFailed(HRESULT hr)
  {
    if (FAILED(hr))
    {
      throw Platform::Exception::CreateException(hr);
    }
  }

  //----------------------------------------------------------------------------
  byte* GetPointerToPixelData(IBuffer^ buffer)
  {
    // Cast to Object^, then to its underlying IInspectable interface.
    Platform::Object^ obj = buffer;
    Microsoft::WRL::ComPtr<IInspectable> insp(reinterpret_cast<IInspectable*>(obj));

    // Query the IBufferByteAccess interface.
    Microsoft::WRL::ComPtr<IBufferByteAccess> bufferByteAccess;
    ThrowIfFailed(insp.As(&bufferByteAccess));

    // Retrieve the buffer data.
    byte* pixels = nullptr;
    ThrowIfFailed(bufferByteAccess->Buffer(&pixels));
    return pixels;
  }
}

namespace UWPOpenIGTLinkUI
{
  //----------------------------------------------------------------------------
  IGTLConnectorPage::IGTLConnectorPage()
  {
    InitializeComponent();

    m_IGTClient->ServerHost = ref new HostName(L"127.0.0.1");
    ServerHostnameTextBox->Text = L"127.0.0.1";
  }

  //----------------------------------------------------------------------------
  void IGTLConnectorPage::OnUITimerTick(Platform::Object^ sender, Platform::Object^ e)
  {
    if (m_IGTClient->Connected)
    {
      double timestamp(0.0);

      VideoFrame^ frame = m_IGTClient->GetImage(timestamp);

      if (frame == nullptr)
      {
          return;
      }

      if (m_WriteableBitmap == nullptr)
      {
        m_WriteableBitmap = ref new WriteableBitmap(frame->Dimensions[0], frame->Dimensions[1]);
      }

      if (!IBufferToWriteableBitmap(frame->Image->ImageData, frame->Dimensions[0], frame->Dimensions[1], frame->NumberOfScalarComponents))
      {
          return;
      }

      if (ImageDisplay->Source != m_WriteableBitmap)
      {
          ImageDisplay->Source = m_WriteableBitmap;
      }

      
      float3 origin = transform(float3(0.f, 0.f, 0.f), transpose(frame->EmbeddedImageTransform));

      Platform::String^ text = L"Receiving video.\n";
 
      TransformTextBlock->Text = text;
      
    }
    else
    {
        Platform::String^ text = L"Video stopped.\n";

        TransformTextBlock->Text = text;
    }
  }

  //----------------------------------------------------------------------------
  void IGTLConnectorPage::ServerPortTextBox_TextChanged(Platform::Object^ sender, TextChangedEventArgs^ e)
  {
    TextBox^ textBox = dynamic_cast<TextBox^>(sender);
    if (textBox->Text != m_IGTClient->ServerPort)
    {
      m_IGTClient->ServerPort = textBox->Text;
    }
  }

  //----------------------------------------------------------------------------
  void IGTLConnectorPage::ServerHostnameTextBox_TextChanged(Platform::Object^ sender, TextChangedEventArgs^ e)
  {
    TextBox^ textBox = dynamic_cast<TextBox^>(sender);
    if (textBox->Text != m_IGTClient->ServerHost->DisplayName)
    {
      m_hostname = textBox->Text;
    }
  }

  //----------------------------------------------------------------------------
  void IGTLConnectorPage::ConnectButton_Click(Platform::Object^ sender, RoutedEventArgs^ e)
  {
    ConnectButton->IsEnabled = false;

    StatusIcon->Source = ref new BitmapImage(ref new Uri("ms-appx:///Assets/glossy-yellow-button-2400px.png"));
    if (m_IGTClient->Connected)
    {
      ConnectButton->Content = L"Disconnecting...";
      m_IGTClient->Disconnect();

      StatusBarTextBlock->Text = L"Disconnect successful!";
      StatusIcon->Source = ref new BitmapImage(ref new Uri("ms-appx:///Assets/glossy-green-button-2400px.png"));
      ConnectButton->Content = L"Connect";
      ConnectButton->IsEnabled = true;
    }
    else
    {
      ConnectButton->Content = L"Connecting...";
      m_IGTClient->ServerHost = ref new HostName(m_hostname);
      create_task(m_IGTClient->ConnectAsync(2.0)).then([this](task<bool> connectTask)
      {
        try
        {
          bool result = connectTask.get();
          ProcessConnectionResult(result);
        }
        catch (...)
        {
          return;
        }
      });
    }
  }

  //----------------------------------------------------------------------------
  void IGTLConnectorPage::ProcessConnectionResult(bool result)
  {
    ConnectButton->IsEnabled = true;
    if (result)
    {
      StatusBarTextBlock->Text = L"Success! Connected to " + m_IGTClient->ServerHost + L":" + m_IGTClient->ServerPort;
      StatusIcon->Source = ref new BitmapImage(ref new Uri("ms-appx:///Assets/glossy-green-button-2400px.png"));
      ConnectButton->Content = L"Disconnect";

      m_UITimer->Tick += ref new EventHandler<Object^>(this, &IGTLConnectorPage::OnUITimerTick);
      TimeSpan t;
      t.Duration = 33; // milliseconds, ~30fps
      m_UITimer->Interval = t;
      m_UITimer->Start();
    }
    else
    {
      m_UITimer->Stop();

      ConnectButton->Content = L"Connect";
      StatusBarTextBlock->Text = L"Unable to connect.";
      StatusIcon->Source = ref new BitmapImage(ref new Uri("ms-appx:///Assets/glossy-red-button-2400px.png"));
    }
  }

  //----------------------------------------------------------------------------
  bool IGTLConnectorPage::IBufferToWriteableBitmap(IBuffer^ data, uint32 width, uint32 height, uint16 numberOfcomponents)
  {
    auto divisor = numberOfcomponents / 4.0; // WriteableBitmap has 4 8-bit components BGRA
    if (data->Length * 1.0 != m_WriteableBitmap->PixelBuffer->Length * divisor)
    {
      OutputDebugStringA("Buffers do not contain the same number of pixels.");
      return false;
    }

    try
    {
      byte* sourceImageData = GetPointerToPixelData(data);
      byte* targetImageData = GetPointerToPixelData(m_WriteableBitmap->PixelBuffer);

      for (unsigned int j = 0; j < height; j++)
      {
        for (unsigned int i = 0; i < width; i++)
        {
          if (numberOfcomponents == 3)
          {
            targetImageData[0] = sourceImageData[0];
            targetImageData[1] = sourceImageData[1];
            targetImageData[2] = sourceImageData[2];
          }
          else if (numberOfcomponents == 1)
          {
            targetImageData[0] = *sourceImageData;
            targetImageData[1] = *sourceImageData;
            targetImageData[2] = *sourceImageData;
          }
          targetImageData[3] = 255U; // set to full alpha

          sourceImageData += numberOfcomponents;
          targetImageData += 4;
        }
      }
    }
    catch (Platform::Exception^ e)
    {
      OutputDebugStringA("Unable to copy network image to UI back buffer.\n");
      OutputDebugStringW(e->Message->Data());
      return false;
    }
    m_WriteableBitmap->Invalidate();

    return true;
  }
}