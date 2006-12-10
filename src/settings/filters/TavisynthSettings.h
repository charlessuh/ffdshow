#ifndef _TAVISYNTHSETTINGS_H_
#define _TAVISYNTHSETTINGS_H_

#include "TfilterSettings.h"

class TffdshowPageDec;
struct TavisynthSettings :TfilterSettingsVideo
{
private:
 static const TfilterIDFF idffs;
protected: 
 virtual const int *getResets(unsigned int pageId);
public: 
 TavisynthSettings(TintStrColl *Icoll=NULL,TfilterIDFFs *filters=NULL);
 char_t script[2048];
 int inYV12,inYUY2,inRGB24,inRGB32;
 int ffdshowSource;
 virtual void createFilters(size_t filtersorder,Tfilters *filters,TfilterQueue &queue) const;
 virtual void createPages(TffdshowPageDec *parent) const;
};

#endif
