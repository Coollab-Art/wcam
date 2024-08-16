// qedit.h - Simple version
#ifndef __QEDIT_H__
#define __QEDIT_H__

#pragma once

#include <dshow.h>

// GUIDs for QEdit interfaces
DEFINE_GUID(CLSID_SampleGrabber, 0xc1f400a0, 0x3f08, 0x11d3, 0x9f0b, 0x00c0, 0x4f, 0xb6, 0xe0, 0x80, 0x8c, 0x24);
DEFINE_GUID(CLSID_NullRenderer, 0xc1f400a4, 0x3f08, 0x11d3, 0x9f0b, 0x00c0, 0x4f, 0xb6, 0xe0, 0x80, 0x8c, 0x24);
DEFINE_GUID(IID_ISampleGrabber, 0x6b652fff, 0x11fe, 0x4fce, 0x92ad, 0x02c, 0x80, 0x1f, 0x9d, 0x53, 0x8b, 0x9);

// Declare interfaces here

interface ISampleGrabberCB : public IUnknown {
    virtual STDMETHODIMP SampleCB(double SampleTime, IMediaSample *pSample) = 0;
    virtual STDMETHODIMP BufferCB(double SampleTime, BYTE *pBuffer, long BufferLen) = 0;
};

interface ISampleGrabber : public IUnknown {
    virtual STDMETHODIMP SetOneShot(BOOL OneShot) = 0;
    virtual STDMETHODIMP SetMediaType(const AM_MEDIA_TYPE *pType) = 0;
    virtual STDMETHODIMP GetConnectedMediaType(AM_MEDIA_TYPE *pType) = 0;
    virtual STDMETHODIMP SetBufferSamples(BOOL BufferThem) = 0;
    virtual STDMETHODIMP GetCurrentBuffer(long *pBufferSize, long *pBuffer) = 0;
    virtual STDMETHODIMP GetCurrentSample(IMediaSample **ppSample) = 0;
    virtual STDMETHODIMP SetCallback(ISampleGrabberCB *pCallback, long WhichMethodToCallback) = 0;
};

#endif // __QEDIT_H__
