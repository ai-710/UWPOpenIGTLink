/*=Plus=header=begin======================================================
Program: Plus
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.

Modified by Adam Rankin, Robarts Research Institute, 2017

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

=========================================================Plus=header=end*/

#pragma once

// Local includes
#include "IGTCommon.h"
#include "TrackedFrame.h"

// OS includes
#include <Windows.h>

// STD includes
#include <string>

// IGTL includes
#include <igtl_types.h>
#include <igtl_win32header.h>
#include <igtlMessageBase.h>
#include <igtlObject.h>
#include <igtlutil/igtl_header.h>

// Windows includes
#include <DirectXMath.h>


using namespace Windows::Foundation::Numerics;

namespace igtl
{
  // This command prevents 4-byte alignment in the struct (which enables m_FrameSize[3])
#pragma pack(1)
  /* For 1-byte boundary in memory */

  ///  TrackedFrameMessage - IGTL message helper class for tracked frame messages
  class TrackedFrameMessage : public MessageBase
  {
  public:
    typedef TrackedFrameMessage                       Self;
    typedef MessageBase                               Superclass;
    typedef SmartPointer<Self>                        Pointer;
    typedef SmartPointer<const Self>                  ConstPointer;
    typedef std::vector<UWPOpenIGTLink::Transform^>   FrameTransformList;

    igtlTypeMacro(igtl::TrackedFrameMessage, igtl::MessageBase);
    igtlNewMacro(igtl::TrackedFrameMessage);

  public:
    /*! Override clone so that we use the plus igtl factory */
    virtual igtl::MessageBase::Pointer Clone();

    /// Accessors to the various parts of the message and message header
    std::shared_ptr<byte> GetImage();
    UWPOpenIGTLink::US_IMAGE_TYPE GetImageType();
    igtl_uint16* GetFrameSize();
    igtl_uint16 GetNumberOfComponents();
    igtl_uint32 GetImageSizeInBytes();
    UWPOpenIGTLink::US_IMAGE_ORIENTATION GetImageOrientation();
    UWPOpenIGTLink::IGTL_SCALAR_TYPE GetScalarType();
    double GetTimestamp();

    /*! Set the embedded transform of the underlying image */
    void SetEmbeddedImageTransform(const Windows::Foundation::Numerics::float4x4& matrix);
    /*! Get the embedded transform of the underlying image */
    Windows::Foundation::Numerics::float4x4 GetEmbeddedImageTransform();

    UWPOpenIGTLink::TransformListInternal GetFrameTransforms();
    void SetFrameTransforms(const UWPOpenIGTLink::TransformListInternal& transforms);
    void ApplyTransformUnitScaling(float scalingFactor);

  protected:
    class TrackedFrameHeader
    {
    public:
      TrackedFrameHeader();

      size_t GetMessageHeaderSize();

      void ConvertEndianness();

      igtl_uint16     m_ScalarType;             /* scalar type */
      igtl_uint16     m_NumberOfComponents;     /* number of scalar components */
      igtl_uint16     m_ImageType;              /* image type */
      igtl_uint16     m_FrameSize[3];           /* entire image volume size */
      igtl_uint32     m_ImageDataSizeInBytes;   /* size of the image, in bytes */
      igtl_uint32     m_XmlDataSizeInBytes;     /* size of the xml data, in bytes */
      igtl_uint16     m_ImageOrientation;       /* orientation of the image */
      igtl::Matrix4x4 m_EmbeddedImageTransform; /* matrix representing the IJK to world transformation */
    };

    virtual igtlUint64                      CalculateContentBufferSize();
    virtual int                             PackContent();
    virtual int                             UnpackContent();

    TrackedFrameMessage();
    ~TrackedFrameMessage();

    FrameTransformList                      m_frameTransforms;
    std::shared_ptr<byte>                   m_image = nullptr;
    std::string                             m_trackedFrameXmlData;
    bool                                    m_imageValid = false;
    double                                  m_timestamp = 0.0;

    TrackedFrameHeader                      m_messageHeader;
  };

#pragma pack()

}