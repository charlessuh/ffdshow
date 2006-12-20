/*
 * Copyright (c) 2003-2006 Milan Cutka
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "stdafx.h"
#include "ffdshowRemoteAPIimpl.h"
#include "IffdshowBase.h"
#include "IffdshowDec.h"
#include "IffdshowDecVideo.h"
#include "TkeyboardDirect.h"
#include "TsubtitlesFile.h"
#include "reg.h"
#include "Tpresets.h"
#include "TpresetSettings.h"


Tremote::Tremote(TintStrColl *Icoll,IffdshowBase *Ideci):deci(Ideci),Toptions(Icoll)
{
 static const TintOptionT<Tremote> iopts[]=
  {
   IDFF_isRemote           ,&Tremote::is           ,0,0,_l(""),0,
     _l("isRemote"),0,
   IDFF_remoteMessageMode  ,&Tremote::messageMode  ,0,1,_l(""),0,
     _l("remoteMessageMode"),0,
   IDFF_remoteMessageUser  ,&Tremote::messageUser  ,1,1,_l(""),0,
     _l("remoteMessageUser"),WM_APP+18,
   IDFF_remoteAcceptKeys   ,&Tremote::acceptKeys   ,1,1,_l(""),0,
     _l("remoteAcceptKeys"),1,
   0
  };
 addOptions(iopts); 
 setOnChange(IDFF_isRemote,this,&Tremote::onChange);
 load();

 h=NULL;
 keys=NULL;hThread=NULL;
 inExplorer=deci->inExplorer()==S_OK;
}
Tremote::~Tremote()
{
 stop();
 if (keys) delete keys;
}

void Tremote::load(void)
{
 TregOpRegRead t(HKEY_CURRENT_USER,FFDSHOW_REG_PARENT _l("\\") FFDSHOWDECVIDEO);
 reg_op(t);
}
void Tremote::save(void)
{
 TregOpRegWrite t(HKEY_CURRENT_USER,FFDSHOW_REG_PARENT _l("\\") FFDSHOWDECVIDEO);
 reg_op(t);
}

void Tremote::onChange(int id,int val)
{
 int filtermode;
 if (is && !inExplorer && ((filtermode=deci->getParam2(IDFF_filterMode))&IDFF_FILTERMODE_PLAYER) && !(filtermode&IDFF_FILTERMODE_VFW))
  start();
 else 
  stop();
}

void Tremote::start(void)
{
 if (hThread) return;
 deciD=deci;
 deciV=deci;
 remotemsg=messageMode==0?RegisterWindowMessage(_l(FFDSHOW_REMOTE_MESSAGE)):messageUser;
 paramid=0;subtitleIdx=0;
 unsigned threadID;
 hThread=(HANDLE)_beginthreadex(NULL,65536,threadProc,this,NULL,&threadID);
}
void Tremote::stop(void)
{
 if (h) 
  {
   SendMessage(h,WM_CLOSE,0,0);
   WaitForSingleObject(hThread,INFINITE);
   hThread=NULL;
  }
 deciD=NULL;deciV=NULL;
}
unsigned int __stdcall Tremote::threadProc(void *self0)
{
 randomize();
 setThreadName(DWORD(-1),"remote");
 
 Tremote *self=(Tremote*)self0;
 HINSTANCE hi=self->deci->getInstance2();
 char_t windowName[80];tsprintf(windowName,_l("%s_window%i"),FFDSHOW_REMOTE_CLASS,rand());
 ATOM at=NULL;
 self->h=createInvisibleWindow(hi,_l(FFDSHOW_REMOTE_CLASS),windowName,remoteWndProc0,self,&at);
 if (self->h)
  {
   SetWindowLongPtr(self->h,GWLP_USERDATA,LONG_PTR(self));
   MSG msg;
   while(GetMessage(&msg, NULL, 0, 0))
    {
     TranslateMessage(&msg);
     DispatchMessage(&msg);
    }
  }  
 UnregisterClass(_l(FFDSHOW_REMOTE_CLASS),hi);
 self->h=NULL;
 _endthreadex(0);
 return 0;
}

LRESULT CALLBACK Tremote::remoteWndProc0(HWND hwnd, UINT msg, WPARAM wprm, LPARAM lprm)
{
 Tremote *self=(Tremote*)GetWindowLongPtr(hwnd,GWLP_USERDATA);
 //DPRINTF("got remote message %i",msg);
 switch (msg)
  { 
   case WM_DESTROY:
    PostQuitMessage(0);
    break;
  }
 return self?self->remoteWndProc(hwnd,msg,wprm,lprm):DefWindowProc(hwnd,msg,wprm,lprm);
}

LRESULT CALLBACK Tremote::remoteWndProc(HWND hwnd, UINT msg, WPARAM wprm, LPARAM lprm)
{
 if (msg==remotemsg)
  switch (wprm)
   {
    case WPRM_SETPARAM_ID:
     paramid=(int)lprm;
     return TRUE;
    case WPRM_GETPARAM:
     return deci->getParam2(paramid);
    case WPRM_GETPARAM2:
     return deci->getParam2((int)lprm);
    case WPRM_PUTPARAM:
     return SUCCEEDED(deci->putParam(paramid,(int)lprm))?TRUE:FALSE;
    case WPRM_STOP:
     return SUCCEEDED(deciD->stop())?TRUE:FALSE;
    case WPRM_RUN:
     return SUCCEEDED(deciD->run())?TRUE:FALSE;
    case WPRM_GETSTATE:
     return deciD->getState2(); 
    case WPRM_GETDURATION:
     return deci->getParam2(IDFF_movieDuration);
    case WPRM_GETCURTIME:
     return deciD->getCurTime2();
    case WPRM_PREVPRESET:
     return SUCCEEDED(deciD->cyclePresets(-1))?TRUE:FALSE;
    case WPRM_NEXTPRESET:
     return SUCCEEDED(deciD->cyclePresets(+1))?TRUE:FALSE;
	case WPRM_SETCURTIME:
     return SUCCEEDED(deciD->seek((int)lprm))?TRUE:FALSE;
   }
 if (acceptKeys && (msg==WM_SYSKEYDOWN || msg==WM_SYSKEYUP || msg==WM_KEYDOWN || msg==WM_KEYUP))
  {
   if (!keys) 
    {
     keys=new Tkeyboard(new TintStrColl,deci);
     keys->load();
    } 
   switch (msg)
    {
     case WM_SYSKEYDOWN:
     case WM_KEYDOWN:
      keys->keyDown((int)wprm);
      break;
     case WM_SYSKEYUP:
     case WM_KEYUP:
      keys->keyUp((int)wprm);
      break;
    }
  }  
 else if (msg==WM_COPYDATA)
  {
   const COPYDATASTRUCT *cds=(const COPYDATASTRUCT*)lprm;
   switch (cds->dwData)
    {
     case COPY_PUTPARAMSTR:
      return SUCCEEDED(deci->putParamStr(paramid,text<char_t>((const char*)cds->lpData)))?TRUE:FALSE;
     case COPY_GETPARAMSTR:
      {
       if (!cds->cbData) return false;
       char_t *buft=(char_t*)alloca(cds->cbData*sizeof(char_t));
       if (SUCCEEDED(deci->getParamStr(paramid,buft,cds->cbData)))
        {
         text<char>(buft,cds->cbData,(char*)cds->lpData);
		 ((COPYDATASTRUCT*)lprm)->dwData = paramid;
		 SendMessage((HWND)wprm, WM_COPYDATA, paramid, lprm);
         return TRUE;
        } 
       return FALSE; 
       //return cds->cbData && SUCCEEDED(deci->getParamStr(paramid,(char*)cds->lpData,cds->cbData))?TRUE:FALSE;
      } 
     case COPY_SETACTIVEPRESET:
      return SUCCEEDED(deciD->setActivePreset(text<char_t>((const char*)cds->lpData),false))?TRUE:FALSE;
     case COPY_AVAILABLESUBTITLE_FIRST:
      subtitleIdx=0;
     case COPY_AVAILABLESUBTITLE_NEXT:
      {
       if (!deciV || cds->cbData==0) return FALSE;
       strings files;
       TsubtitlesFile::findPossibleSubtitles(deci->getSourceName(),deci->getParamStr2(IDFF_subSearchDir),files);
       if (subtitleIdx>=files.size())
        ((char*)cds->lpData)[0]='\0';
       else
        {
         strncpy((char*)cds->lpData,text<char>(files[subtitleIdx].c_str()),cds->cbData);
         ((char*)cds->lpData)[cds->cbData-1]='\0';
         subtitleIdx++;
        }
	   SendMessage((HWND)wprm, WM_COPYDATA, paramid, lprm);
       return TRUE;  
      }
	  case COPY_GET_PRESETLIST:
	  {
		COPYDATASTRUCT cd;
		cd.dwData = COPY_GET_PRESETLIST;
		Tpresets *presets;
		deciD->getPresetsPtr(&presets);
		int presetsNum = presets->size();
		size_t string_size = 2048;
		char_t *presetList = (char_t*)alloca(sizeof(char_t)*string_size);
		strcpy(presetList, _l(""));
		for (int i=0; i<presetsNum; i++)
		{
			Tpreset *preset = presets->at(i);
			const char_t *presetName = preset->presetName;
			// Resize the string if needed
			if (strlen(presetList)+strlen(presetName)+ 1 >= string_size)
			{
				string_size += 2048;
				char_t *tmpStr = (char_t*)alloca(sizeof(char_t)*string_size);
				strcpy(tmpStr, presetList);
				free(presetList); presetList = tmpStr;
			}
			strcat(presetList, presetName);
			if (i != presetsNum - 1)
				strcat(presetList, _l(";"));
		}
		cd.lpData = alloca(sizeof(char)*(strlen(presetList)+1));
		strcpy((char*)cd.lpData, "");
		text<char>(presetList, (char*)cd.lpData);
		cd.cbData = strlen(presetList)+1;
		SendMessage((HWND)wprm, WM_COPYDATA, COPY_GET_PRESETLIST, (LPARAM)&cd);
		return TRUE;
	  }
	  case COPY_GET_SOURCEFILE:
	  {
		COPYDATASTRUCT cd;
		cd.dwData = COPY_GET_SOURCEFILE;
		const char_t *fileName = deci->getSourceName();
		cd.lpData = alloca(sizeof(char)*(strlen(fileName)+1));
		strcpy((char*)cd.lpData, "");
		text<char>(fileName, (char*)cd.lpData);
		cd.cbData = strlen(fileName)+1;
		SendMessage((HWND)wprm, WM_COPYDATA, COPY_GET_SOURCEFILE, (LPARAM)&cd);
		return TRUE;
	  }
    }
  }
 return DefWindowProc(hwnd,msg,wprm,lprm); 
}
