/*
 * svccli.c: Sample service client plugin
 *
 * See the README file for copyright information and how to reach the author.
 *
 * $Id: svccli.c 5.2 2026/01/21 20:54:47 kls Exp $
 */

#include <stdlib.h>
#include <vdr/interface.h>
#include <vdr/plugin.h>

static const char *VERSION        = "2.6.0";
static const char *DESCRIPTION    = "Service demo client";
static const char *MAINMENUENTRY  = "Service demo";

class cPluginSvcCli : public cPlugin {
public:
  virtual const char *Version(void) override { return VERSION; }
  virtual const char *Description(void) override { return DESCRIPTION; }
  virtual const char *MainMenuEntry(void) override { return MAINMENUENTRY; }
  virtual cOsdObject *MainMenuAction(void) override;
  virtual bool Service(const char *Id, void *Data) override;
  };

struct ReportBoredPlugin_v1_0 {
  cPlugin *BoredPlugin;
  };

struct AddService_v1_0 {
  int a, b;
  int sum;
  };

// --- cPluginSvcCli ---------------------------------------------------------

cOsdObject *cPluginSvcCli::MainMenuAction(void)
{
  char s[128];
  cPlugin *p;

  // Inform server plugin that we are bored
  // (directed communication)
  ReportBoredPlugin_v1_0 rbp;
  rbp.BoredPlugin = this;
  p = cPluginManager::GetPlugin("svcsvr");
  if (p)
     p->Service("ReportBoredPlugin-v1.0", &rbp);

  // See if any plugin can add
  // (detect capability)
  p = cPluginManager::CallFirstService("AddService-v1.0", NULL);
  if (p) {
     snprintf(s, sizeof(s), "Plugin %s can add", p->Name());
     Interface->Confirm(s);
     }

  // Perform add
  // (use general service)
  AddService_v1_0 adds;
  adds.a = 1;
  adds.b = 1;
  if (cPluginManager::CallFirstService("AddService-v1.0", &adds)) {
     snprintf(s, sizeof(s), "Plugin thinks that 1+1=%i", adds.sum);
     Interface->Confirm(s);
     }

  // Inform other plugins that we are bored
  // (broadcast)
  rbp.BoredPlugin = this;
  cPluginManager::CallAllServices("ReportBoredPlugin-v1.0", &rbp);

  return NULL;
}

bool cPluginSvcCli::Service(const char *Id, void *Data)
{
  if (strcmp(Id, "ReportBoredPlugin-v1.0") == 0) {
     if (Data) {
        ReportBoredPlugin_v1_0 *rbp = (ReportBoredPlugin_v1_0*)Data;
        char s[128];
        snprintf(s, sizeof(s), "Plugin %s informed client that it is bored.", rbp->BoredPlugin->Name());
        Interface->Confirm(s);
        }
     return true;
     }
  return false;
}

VDRPLUGINCREATOR(cPluginSvcCli); // Don't touch this!
