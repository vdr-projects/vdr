/*
 * svcsvr.c: Sample service server plugin
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: svcsvr.c 5.1 2025/03/02 11:03:35 kls Exp $
 */

#include <stdlib.h>
#include <vdr/interface.h>
#include <vdr/plugin.h>

static const char *VERSION        = "2.4.0";
static const char *DESCRIPTION    = "Service demo server";

class cPluginSvcSvr : public cPlugin {
public:
  virtual const char *Version(void) override { return VERSION; }
  virtual const char *Description(void) override { return DESCRIPTION; }
  virtual bool Service(const char *Id, void *Data) override;
  };

struct ReportBoredPlugin_v1_0 {
  cPlugin *BoredPlugin;
  };

struct AddService_v1_0 {
  int a, b;
  int sum;
  };

// --- cPluginSvcSvr ---------------------------------------------------------

bool cPluginSvcSvr::Service(const char *Id, void *Data)
{
  if (strcmp(Id,"ReportBoredPlugin-v1.0") == 0) {
     if (Data) {
        ReportBoredPlugin_v1_0 *rbp = (ReportBoredPlugin_v1_0*)Data;
        char s[128];
        snprintf(s, sizeof(s), "Plugin %s informed server that it is bored.", rbp->BoredPlugin->Name());
        Interface->Confirm(s);
        }
     return true;
     }

  if (strcmp(Id,"AddService-v1.0") == 0) {
     if (Data) {
        AddService_v1_0 *data = (AddService_v1_0*)Data;
        data->sum = data->a + data->b;
        }
     return true;
     }

  return false;
}

VDRPLUGINCREATOR(cPluginSvcSvr); // Don't touch this!
