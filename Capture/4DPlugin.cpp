/* --------------------------------------------------------------------------------
 #
 #	4DPlugin.c
 #	source generated by 4D Plugin Wizard
 #	Project : Capture
 #	author : miyako
 #	2013/03/04
 #
 # --------------------------------------------------------------------------------*/

#include "4DPluginAPI.h"
#include "4DPlugin.h"

#include "dshow.h"
#include "qedit.h"
#pragma comment(lib, "strmiids")

BOOL _isComReady = FALSE;

typedef struct{
	IVideoWindow	*videoWindow;
	IMediaControl	*mediaControl;
	IGraphBuilder	*graphBuilder;
	IBaseFilter		*deviceFilter;
	IBaseFilter		*grabberFilter;	
	ISampleGrabber	*sampleGrabber;
}captureContext;

std::map<uint32_t, captureContext*> _previews;
std::map<uint32_t, CUTF16String> _devices;

captureContext *_contextGet(C_LONGINT &preview){
	
	captureContext *context = NULL;
	
	unsigned int i = preview.getIntValue();
	
	std::map<uint32_t, captureContext*>::iterator pos = _previews.find(i);
	
	if(pos != _previews.end()){
		context = pos->second;
	}
	
	return context;
}

void _contextDelete(C_LONGINT &preview){
	
	captureContext *context = NULL;
	
	unsigned int i = preview.getIntValue();
	
	std::map<uint32_t, captureContext*>::iterator pos = _previews.find(i);
	
	if(pos != _previews.end()){
		
		context = pos->second;

		context->videoWindow->Release();
		context->mediaControl->Release();
		context->sampleGrabber->Release();
		context->grabberFilter->Release();
		context->graphBuilder->Release();
		context->deviceFilter->Release();
		
		delete context;

		_previews.erase(pos);
		
		std::map<uint32_t, CUTF16String>::iterator pos = _devices.find(i);
		if(pos != _devices.end())
			_devices.erase(pos);
	}
	
}

IBaseFilter *_getDeviceForName(C_TEXT &name)
{
	IBaseFilter *pBFilter = NULL;
	
	if(_isComReady)
    {
		ICreateDevEnum *pDevEnum = NULL;
		HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, 
									  NULL,  
									  CLSCTX_INPROC_SERVER, 
									  IID_PPV_ARGS(&pDevEnum));
		
		CUTF16String n;
		name.copyUTF16String(&n);
		
		if(SUCCEEDED(hr))
		{
			IEnumMoniker *pEnum = NULL;
			hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
			
			if(hr != S_FALSE)
			{
				IMoniker *pMoniker = NULL;
				while(pEnum->Next(1, &pMoniker, NULL) == S_OK)
				{
					IPropertyBag *pPropBag = NULL;
					hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
					
					if(FAILED(hr))
					{
						pMoniker->Release();
						continue;  
					} 
					
					VARIANT var;
					VariantInit(&var);
					
					CUTF16String description;
					
					hr = pPropBag->Read(L"FriendlyName", &var, 0);
					
					if(SUCCEEDED(hr))
					{
						description = (const PA_Unichar *)var.bstrVal;
						VariantClear(&var);
						
						if(n.compare(description) == 0)
						{
							hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void **)&pBFilter);
						}
						
					}
					
					pPropBag->Release();
					pMoniker->Release();
				}	
				
			}
			pDevEnum->Release();
		}
		
	}
	return pBFilter;
}

captureContext *_contextCreate(C_TEXT &device, C_LONGINT &preview){
	
	captureContext *context = NULL;

	if(_isComReady)
    {
		CUTF16String deviceName;
		device.copyUTF16String(&deviceName);
		
		BOOL _isAlreadyUsed = FALSE;
		
		for(std::map<uint32_t, CUTF16String>::iterator it = _devices.begin(); it != _devices.end(); ++it){
			if(it->second.compare(deviceName) == 0){
				_isAlreadyUsed = TRUE;
				preview.setIntValue(it->first);
				context = _contextGet(preview);
				break;
			}
		}
		
		if(!_isAlreadyUsed){
			IBaseFilter *pDeviceFilter = _getDeviceForName(device);
			if(pDeviceFilter)
			{
				IGraphBuilder *pGraph = NULL;
				HRESULT hr = CoCreateInstance(CLSID_FilterGraph, NULL,
											  CLSCTX_INPROC_SERVER,
											  IID_IGraphBuilder,
											  (void **)&pGraph);	
				if(SUCCEEDED(hr)){
					ICaptureGraphBuilder2 *pCapture = NULL;
					hr = CoCreateInstance(CLSID_CaptureGraphBuilder2,
										  NULL,
										  CLSCTX_INPROC_SERVER,
										  IID_ICaptureGraphBuilder2,
										  (void**)&pCapture);
					
					if(SUCCEEDED(hr)){
						IBaseFilter *pGrabberFilter = NULL;
						hr = CoCreateInstance(CLSID_SampleGrabber, 
											  NULL, 
											  CLSCTX_INPROC_SERVER,
											  IID_PPV_ARGS(&pGrabberFilter));
						if(SUCCEEDED(hr)){
							ISampleGrabber *pGrabber = NULL;
							hr = pGrabberFilter->QueryInterface(IID_ISampleGrabber, (void **)&pGrabber);
							if(SUCCEEDED(hr)){
								//Set the Media Type
								AM_MEDIA_TYPE mt;
								ZeroMemory(&mt, sizeof(mt));
								mt.majortype = MEDIATYPE_Video;
								mt.subtype = MEDIASUBTYPE_RGB24;
								mt.formattype = FORMAT_VideoInfo;
								hr = pGrabber->SetMediaType(&mt);
								if(SUCCEEDED(hr)){
									hr = pGraph->AddFilter(pGrabberFilter, L"Sample Grabber");	
									if(SUCCEEDED(hr)){
										hr = pCapture->SetFiltergraph(pGraph);
										if(SUCCEEDED(hr)){
											IMediaControl *pMControl = NULL;
											hr = pGraph->QueryInterface(IID_IMediaControl, (void **)&pMControl);
											if(SUCCEEDED(hr)){
												hr = pGraph->AddFilter(pDeviceFilter, L"Device Filter");
												if(SUCCEEDED(hr)){													
													hr = pCapture->RenderStream(&PIN_CATEGORY_PREVIEW, 
																				NULL, 
																				pDeviceFilter, 
																				pGrabberFilter, 
																				NULL);						
													if(SUCCEEDED(hr)){
														IVideoWindow *pVWindow = NULL;
														hr = pGraph->QueryInterface(IID_IVideoWindow, (void **)&pVWindow);
														if(SUCCEEDED(hr)){
															
															pVWindow->put_Visible(OAFALSE);
															pVWindow->put_AutoShow(OAFALSE);
															pVWindow->SetWindowPosition(0, 0, 0, 0);
															pVWindow->HideCursor(OAFALSE);
															pVWindow->put_WindowStyle(WS_CHILD | WS_CLIPCHILDREN);
															
															unsigned int i = 1;
															
															while (_previews.find(i) != _previews.end()) {
																i++;
															}
															
															context = new(captureContext);
															
															context->mediaControl = pMControl;
															context->videoWindow = pVWindow;
															context->graphBuilder = pGraph;
															context->deviceFilter = pDeviceFilter;
															context->grabberFilter = pGrabberFilter;
															context->sampleGrabber = pGrabber;
															
															_previews.insert(std::map<uint32_t, captureContext*>::value_type(i, context));
															_devices.insert(std::map<uint32_t, CUTF16String>::value_type(i, deviceName));
															
															preview.setIntValue(i);
															
															context->mediaControl->Pause();	
															context->sampleGrabber->SetBufferSamples(FALSE);	
															
														}//pVWindow
													}//RenderStream
												}//AddFilter
											}//pMControl
										}//SetFiltergraph
									}//AddFilter
								}//SetMediaType
							}//pGrabber
						}//pGrabberFilter
						pCapture->Release();
					}//pCapture
				}//pGraph
			}//pDeviceFilter
		}
	}
	return context;
}

void _getDevicesList(ARRAY_TEXT &names)
{
	
	names.setSize(0);	
	names.setSize(1);
	
	if(_isComReady)
    {
		ICreateDevEnum *pDevEnum = NULL;
		HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, 
									  NULL,
									  CLSCTX_INPROC_SERVER, 
									  IID_PPV_ARGS(&pDevEnum));
		
		if(SUCCEEDED(hr))
		{
			IEnumMoniker *pEnum = NULL;
			hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
			
			if(hr != S_FALSE)
			{
				IMoniker *pMoniker = NULL;
				while(pEnum->Next(1, &pMoniker, NULL) == S_OK)
				{
					IPropertyBag *pPropBag = NULL;
					hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
					
					if(FAILED(hr))
					{
						pMoniker->Release();
						continue;  
					} 
					
					VARIANT var;
					VariantInit(&var);
					
					CUTF16String name;
					
					hr = pPropBag->Read(L"FriendlyName", &var, 0);
					
					if(SUCCEEDED(hr))
					{
						name = (const PA_Unichar *)var.bstrVal;
						VariantClear(&var);
						names.appendUTF16String(&name);
					}
					
					pPropBag->Release();
					pMoniker->Release();
				}
				
			}
			pDevEnum->Release();
		}
	}
}

void _getDefaultDevice(C_TEXT &name)
{
	ARRAY_TEXT names;
	
	_getDevicesList(names);
	
	if(names.getSize() > 1){
		CUTF16String n;
		names.copyUTF16StringAtIndex(&n, 1);
		name.setUTF16String(&n);
	}else{
		name.setUTF16String((const PA_Unichar *)L"", 0);
	}
}

void _getRect(C_LONGINT &paramPreview, C_LONGINT&paramX, C_LONGINT&paramY, C_LONGINT&paramW, C_LONGINT&paramH)
{
	captureContext *context = _contextGet(paramPreview);
	
	if(context){
		
		long x = 0, y = 0, w = 0, h = 0;
		
		if(S_OK == context->videoWindow->GetWindowPosition(&x, &y, &w, &h)){
			paramX.setIntValue(x);
			paramY.setIntValue(y);
			paramW.setIntValue(w);
			paramH.setIntValue(h);	
		}
	}
}

void _setRect(C_LONGINT &paramPreview, C_LONGINT&paramX, C_LONGINT&paramY, C_LONGINT&paramW, C_LONGINT&paramH)
{
	captureContext *context = _contextGet(paramPreview);
	
	if(context){
		
		long x = 0, y = 0, w = 0, h = 0;
		
		context->videoWindow->SetWindowPosition(paramX.getIntValue(), 
								   paramY.getIntValue(), 
								   paramW.getIntValue(), 
								   paramH.getIntValue());		
	}	
}

void _getVisible(C_LONGINT &paramPreview, C_LONGINT&paramVisible)
{
	captureContext *context = _contextGet(paramPreview);
	
	if(context){
		
		long v = 0;
		
		if(S_OK == context->videoWindow->get_Visible(&v)){
			paramVisible.setIntValue(v);	
		}
	}	
}

void _setVisible(C_LONGINT &paramPreview, C_LONGINT&paramVisible)
{
	captureContext *context = _contextGet(paramPreview);
	
	if(context){		
		if(paramVisible.getIntValue()){
			context->sampleGrabber->SetBufferSamples(TRUE);	
			context->videoWindow->put_Visible(OATRUE);
			context->videoWindow->put_AutoShow(OATRUE);
			context->mediaControl->Run();
		}else{
			context->sampleGrabber->SetBufferSamples(FALSE);	
			context->videoWindow->put_Visible(OAFALSE);
			context->videoWindow->put_AutoShow(OAFALSE);
			context->mediaControl->Pause();			
		}
	}	
}

void _getWindow(C_LONGINT &paramPreview, C_LONGINT&paramWindow)
{
	captureContext *context = _contextGet(paramPreview);
	
	if(context){
		
		OAHWND owner = 0;
		
		if(S_OK == context->videoWindow->get_Owner(&owner)){
			paramWindow.setIntValue(owner);	
		}
	}	
}

void _setWindow(C_LONGINT &paramPreview, C_LONGINT&paramWindow)
{
	captureContext *context = _contextGet(paramPreview);
	
	if(context){		
		context->videoWindow->put_Owner((OAHWND)PA_GetHWND((PA_WindowRef)paramWindow.getIntValue()));
	}	
}

void _getFullscreen(C_LONGINT &paramPreview, C_LONGINT&fullScreen)
{
	captureContext *context = _contextGet(paramPreview);
	
	if(context){
		
		long f = 0;
		
		if(S_OK == context->videoWindow->get_FullScreenMode(&f)){
			fullScreen.setIntValue(f);	
		}
	}	
}


void _setFullscreen(C_LONGINT &paramPreview, C_LONGINT&fullScreen)
{
	captureContext *context = _contextGet(paramPreview);
	
	if(context){		
		if(fullScreen.getIntValue()){
			context->videoWindow->put_FullScreenMode(OATRUE);
		}else{
			context->videoWindow->put_FullScreenMode(OAFALSE);	
		}
	}	
}

HWND _getMdi()
{
	HWND mdi = NULL;
	
	unsigned int len, i;
	PA_Unistring applicationPath = PA_GetApplicationFullPath();
	len = PA_GetUnicharsLength(applicationPath.fString);
	
	for(i = len; i > 0 ;i--)
	{
		applicationPath.fString[i] = 0;
		mdi = FindWindow((LPCTSTR)applicationPath.fString, NULL); 
		if(mdi)	break;
	}
	
	return mdi;
}

void PluginMain(int32_t selector, PA_PluginParameters params)
{
	try
	{
		int32_t pProcNum = selector;
		sLONG_PTR *pResult = (sLONG_PTR *)params->fResult;
		PackagePtr pParams = (PackagePtr)params->fParameters;

		CommandDispatcher(pProcNum, pResult, pParams); 
	}
	catch(...)
	{

	}
}

void CommandDispatcher (int32_t pProcNum, sLONG_PTR *pResult, PackagePtr pParams)
{
	switch(pProcNum)
	{
		case kInitPlugin :
		case kServerInitPlugin :	
			//COINIT_MULTITHREADED will fail in 4D plugin
			if(SUCCEEDED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))){
				_isComReady = TRUE;
			}
			break;	
			
		case kDeinitPlugin :
		case kServerDeinitPlugin :
			if(_isComReady){
				CoUninitialize();
				_isComReady = FALSE;
			}
			break;				
			// --- Device
			
		case 1 :
			CAPTURE_DEVICE_Get_default(pResult, pParams);
			break;
			
		case 2 :
			CAPTURE_DEVICE_LIST(pResult, pParams);
			break;
			
			// --- Preview
			
		case 3 :
			CAPTURE_PREVIEW_SET_RECT(pResult, pParams);
			break;
			
		case 4 :
			CAPTURE_PREVIEW_SNAP(pResult, pParams);
			break;
			
		case 5 :
			CAPTURE_PREVIEW_CLEAR(pResult, pParams);
			break;
			
		case 6 :
			CAPTURE_PREVIEW_Get_window(pResult, pParams);
			break;
			
		case 7 :
			CAPTURE_PREVIEW_SET_WINDOW(pResult, pParams);
			break;
			
		case 8 :
			CAPTURE_PREVIEW_Get_visible(pResult, pParams);
			break;
			
		case 9 :
			CAPTURE_PREVIEW_SET_VISIBLE(pResult, pParams);
			break;
			
		case 10 :
			CAPTURE_PREVIEW_Create(pResult, pParams);
			break;
			
		case 11 :
			CAPTURE_PREVIEW_GET_RECT(pResult, pParams);
			break;
	}
}

// ------------------------------------ Device ------------------------------------

void CAPTURE_DEVICE_Get_default(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_TEXT returnValue;

	_getDefaultDevice(returnValue);

	returnValue.setReturn(pResult);
}

void CAPTURE_DEVICE_LIST(sLONG_PTR *pResult, PackagePtr pParams)
{
	ARRAY_TEXT names;

	_getDevicesList(names);

	names.toParamAtIndex(pParams, 1);
}

// ------------------------------------ Preview -----------------------------------

IPin *_getFilterPinForDirection(IBaseFilter *pDeviceFilter, PIN_DIRECTION dir){
	
	IPin *filterPin = NULL;
	IEnumPins *pEnum = NULL;
	
	HRESULT hr = pDeviceFilter->EnumPins(&pEnum);
	
	if(SUCCEEDED(hr)){
		IPin *pPin = NULL;
		while (S_OK == pEnum->Next(1, &pPin, NULL)){
			IPin *pTmp = NULL;
			hr = pPin->ConnectedTo(&pTmp);
			if(!SUCCEEDED(hr)){
				if(hr == VFW_E_NOT_CONNECTED){
					PIN_DIRECTION pinDir;
					hr = pPin->QueryDirection(&pinDir);
					if(SUCCEEDED(hr)){
						if(pinDir == dir){
							filterPin = pPin;
							break;
						}
					}
				}
			}else{
				pTmp->Release();
			}	
		}
		pEnum->Release();
	}
	return filterPin;
}

void CAPTURE_PREVIEW_SNAP(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_LONGINT paramPreview;
	C_PICTURE paramSnap;

	paramPreview.fromParamAtIndex(pParams, 1);
	paramSnap.fromParamAtIndex(pParams, 2);

	captureContext *context = _contextGet(paramPreview);
	
	if(context){	

		IGraphBuilder *pGraph = context->graphBuilder;
		IMediaControl *pControl = context->mediaControl;
		IBaseFilter *pDeviceFilter = context->deviceFilter;
		IBaseFilter *pGrabberFilter = context->grabberFilter;
		ISampleGrabber *pSampleGrabber = context->sampleGrabber;
				
		AM_MEDIA_TYPE am_media_type;
		ZeroMemory(&am_media_type, sizeof(am_media_type));
		
		pSampleGrabber->GetConnectedMediaType(&am_media_type);
		
		VIDEOINFOHEADER *pVideoInfoHeader = (VIDEOINFOHEADER *)am_media_type.pbFormat;
		
		long size = am_media_type.lSampleSize;
		std::vector<uint8_t> buf(size);
		
		pSampleGrabber->GetCurrentBuffer(&size, (long *)&buf[0]);
		
		BITMAPFILEHEADER bmphdr;
		memset(&bmphdr, 0, sizeof(bmphdr));
		bmphdr.bfType = ('M' << 8) | 'B';
		bmphdr.bfSize = sizeof(bmphdr) + sizeof(BITMAPINFOHEADER) + size;
		bmphdr.bfOffBits = sizeof(bmphdr) + sizeof(BITMAPINFOHEADER);
		
		C_BLOB imageData;
		imageData.addBytes((const uint8_t *)&bmphdr, sizeof(bmphdr));
		imageData.addBytes((const uint8_t *)&pVideoInfoHeader->bmiHeader, sizeof(BITMAPINFOHEADER));
		imageData.addBytes((const uint8_t *)&buf[0], size);
		
		paramSnap.setBytes(imageData.getBytesPtr(), imageData.getBytesLength());
		paramSnap.toParamAtIndex(pParams, 2);

	}

}

void CAPTURE_PREVIEW_Create(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_TEXT paramDevice;
	C_LONGINT returnValue;

	paramDevice.fromParamAtIndex(pParams, 1);

	_contextCreate(paramDevice, returnValue);

	returnValue.setReturn(pResult);
}

void CAPTURE_PREVIEW_CLEAR(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_LONGINT paramPreview;
	
	paramPreview.fromParamAtIndex(pParams, 1);
	
	_contextDelete(paramPreview);
	
}

void CAPTURE_PREVIEW_Get_visible(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_LONGINT paramPreview;
	C_LONGINT returnValue;
	
	paramPreview.fromParamAtIndex(pParams, 1);
	
	_getVisible(paramPreview, returnValue);
	
	_contextGet(paramPreview);
	
	returnValue.setReturn(pResult);
}

void CAPTURE_PREVIEW_SET_VISIBLE(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_LONGINT paramPreview;
	C_LONGINT paramVisible;
	
	paramPreview.fromParamAtIndex(pParams, 1);
	paramVisible.fromParamAtIndex(pParams, 2);
	
	_setVisible(paramPreview, paramVisible);
}

void CAPTURE_PREVIEW_GET_RECT(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_LONGINT paramPreview;
	C_LONGINT paramX;
	C_LONGINT paramY;
	C_LONGINT paramW;
	C_LONGINT paramH;

	paramPreview.fromParamAtIndex(pParams, 1);

	_getRect(paramPreview, paramX, paramY, paramW, paramH);

	paramX.toParamAtIndex(pParams, 2);
	paramY.toParamAtIndex(pParams, 3);
	paramW.toParamAtIndex(pParams, 4);
	paramH.toParamAtIndex(pParams, 5);
}

void CAPTURE_PREVIEW_SET_RECT(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_LONGINT paramPreview;
	C_LONGINT ParamX;
	C_LONGINT ParamY;
	C_LONGINT ParamW;
	C_LONGINT ParamH;
	
	paramPreview.fromParamAtIndex(pParams, 1);
	ParamX.fromParamAtIndex(pParams, 2);
	ParamY.fromParamAtIndex(pParams, 3);
	ParamW.fromParamAtIndex(pParams, 4);
	ParamH.fromParamAtIndex(pParams, 5);
	
	_setRect(paramPreview, ParamX, ParamY, ParamW, ParamH);
	
}

void CAPTURE_PREVIEW_Get_window(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_LONGINT paramPreview;
	C_LONGINT returnValue;
	
	paramPreview.fromParamAtIndex(pParams, 1);
	
	_getWindow(paramPreview, returnValue);
	
	returnValue.setReturn(pResult);
}

void CAPTURE_PREVIEW_SET_WINDOW(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_LONGINT paramPreview;
	C_LONGINT paramWindow;
	
	paramPreview.fromParamAtIndex(pParams, 1);
	paramWindow.fromParamAtIndex(pParams, 2);
	
	_setWindow(paramPreview, paramWindow);
	
}