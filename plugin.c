/*
 * plugin.c: The VDR plugin interface
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: plugin.c 4.3 2020/12/13 10:56:36 kls Exp $
 */

#include "plugin.h"
#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <time.h>
#include "config.h"
#include "interface.h"
#include "thread.h"

#define LIBVDR_PREFIX  "libvdr-"
#define SO_INDICATOR   ".so."

#define MAXPLUGINARGS  1024
#define HOUSEKEEPINGDELTA 10 // seconds

// --- cPlugin ---------------------------------------------------------------

cString cPlugin::configDirectory;
cString cPlugin::cacheDirectory;
cString cPlugin::resourceDirectory;

cPlugin::cPlugin(void)
{
  name = NULL;
  started = false;
}

cPlugin::~cPlugin()
{
}

void cPlugin::SetName(const char *s)
{
  name = s;
  I18nRegister(name);
}

const char *cPlugin::CommandLineHelp(void)
{
  return NULL;
}

bool cPlugin::ProcessArgs(int argc, char *argv[])
{
  return true;
}

bool cPlugin::Initialize(void)
{
  return true;
}

bool cPlugin::Start(void)
{
  return true;
}

void cPlugin::Stop(void)
{
}

void cPlugin::Housekeeping(void)
{
}

void cPlugin::MainThreadHook(void)
{
}

cString cPlugin::Active(void)
{
  return NULL;
}

time_t cPlugin::WakeupTime(void)
{
  return 0;
}

const char *cPlugin::MainMenuEntry(void)
{
  return NULL;
}

cOsdObject *cPlugin::MainMenuAction(void)
{
  return NULL;
}

cMenuSetupPage *cPlugin::SetupMenu(void)
{
  return NULL;
}

bool cPlugin::SetupParse(const char *Name, const char *Value)
{
  return false;
}

void cPlugin::SetupStore(const char *Name, const char *Value)
{
  Setup.Store(Name, Value, this->Name());
}

void cPlugin::SetupStore(const char *Name, int Value)
{
  Setup.Store(Name, Value, this->Name());
}

bool cPlugin::Service(const char *Id, void *Data)
{
  return false;
}

const char **cPlugin::SVDRPHelpPages(void)
{
  return NULL;
}

cString cPlugin::SVDRPCommand(const char *Command, const char *Option, int &ReplyCode)
{
  return NULL;
}

void cPlugin::SetConfigDirectory(const char *Dir)
{
  configDirectory = Dir;
}

const char *cPlugin::ConfigDirectory(const char *PluginName)
{
  static cString buffer;
  if (!cThread::IsMainThread())
     esyslog("ERROR: plugin '%s' called cPlugin::ConfigDirectory(), which is not thread safe!", PluginName ? PluginName : "<no name given>");
  buffer = cString::sprintf("%s/plugins%s%s", *configDirectory, PluginName ? "/" : "", PluginName ? PluginName : "");
  return MakeDirs(buffer, true) ? *buffer : NULL;
}

void cPlugin::SetCacheDirectory(const char *Dir)
{
  cacheDirectory = Dir;
}

const char *cPlugin::CacheDirectory(const char *PluginName)
{
  static cString buffer;
  if (!cThread::IsMainThread())
     esyslog("ERROR: plugin '%s' called cPlugin::CacheDirectory(), which is not thread safe!", PluginName ? PluginName : "<no name given>");
  buffer = cString::sprintf("%s/plugins%s%s", *cacheDirectory, PluginName ? "/" : "", PluginName ? PluginName : "");
  return MakeDirs(buffer, true) ? *buffer : NULL;
}

void cPlugin::SetResourceDirectory(const char *Dir)
{
  resourceDirectory = Dir;
}

const char *cPlugin::ResourceDirectory(const char *PluginName)
{
  static cString buffer;
  if (!cThread::IsMainThread())
     esyslog("ERROR: plugin '%s' called cPlugin::ResourceDirectory(), which is not thread safe!", PluginName ? PluginName : "<no name given>");
  buffer = cString::sprintf("%s/plugins%s%s", *resourceDirectory, PluginName ? "/" : "", PluginName ? PluginName : "");
  return MakeDirs(buffer, true) ? *buffer : NULL;
}

// --- cDll ------------------------------------------------------------------

cDll::cDll(const char *FileName, const char *Args)
{
  fileName = strdup(FileName);
  args = Args ? strdup(Args) : NULL;
  handle = NULL;
  plugin = NULL;
  destroy = NULL;
}

cDll::~cDll()
{
  if (destroy)
     destroy(plugin);
  else
     delete plugin; // silently fall back for plugins compiled with VDR version <= 2.4.3
  if (handle)
     dlclose(handle);
  free(args);
  free(fileName);
}

static char *SkipQuote(char *s)
{
  char c = *s;
  memmove(s, s + 1, strlen(s));
  while (*s && *s != c) {
        if (*s == '\\')
           memmove(s, s + 1, strlen(s));
        if (*s)
           s++;
        }
  if (*s) {
     memmove(s, s + 1, strlen(s));
     return s;
     }
  esyslog("ERROR: missing closing %c", c);
  fprintf(stderr, "vdr: missing closing %c\n", c);
  return NULL;
}

bool cDll::Load(bool Log)
{
  if (Log)
     isyslog("loading plugin: %s", fileName);
  if (handle) {
     esyslog("attempt to load plugin '%s' twice!", fileName);
     return false;
     }
  handle = dlopen(fileName, RTLD_NOW);
  const char *error = dlerror();
  if (!error) {
     typedef cPlugin *create_t(void);
     create_t *create = (create_t *)dlsym(handle, "VDRPluginCreator");
     error = dlerror();
     if (!error && create) {
        plugin = create();
        destroy = (destroy_t *)dlsym(handle, "VDRPluginDestroyer");
        error = dlerror();
        }
     }
  if (!error) {
     if (plugin && args) {
        int argc = 0;
        char *argv[MAXPLUGINARGS];
        char *p = skipspace(stripspace(args));
        char *q = NULL;
        bool done = false;
        while (!done) {
              if (!q)
                 q = p;
              switch (*p) {
                case '\\': memmove(p, p + 1, strlen(p));
                           if (*p)
                              p++;
                           else {
                              esyslog("ERROR: missing character after \\");
                              fprintf(stderr, "vdr: missing character after \\\n");
                              return false;
                              }
                           break;
                case '"':
                case '\'': if ((p = SkipQuote(p)) == NULL)
                              return false;
                           break;
                default: if (!*p || isspace(*p)) {
                            done = !*p;
                            *p = 0;
                            if (q) {
                               if (argc < MAXPLUGINARGS - 1)
                                  argv[argc++] = q;
                               else {
                                  esyslog("ERROR: plugin argument list too long");
                                  fprintf(stderr, "vdr: plugin argument list too long\n");
                                  return false;
                                  }
                               q = NULL;
                               }
                            }
                         if (!done)
                            p = *p ? p + 1 : skipspace(p + 1);
                }
              }
        argv[argc] = NULL;
        if (argc)
           plugin->SetName(argv[0]);
        optind = 0; // to reset the getopt() data
        return !Log || !argc || plugin->ProcessArgs(argc, argv);
        }
     }
  else {
     esyslog("ERROR: %s", error);
     fprintf(stderr, "vdr: %s\n", error);
     }
  return !error && plugin;
}

// --- cPluginManager --------------------------------------------------------

cPluginManager *cPluginManager::pluginManager = NULL;

cPluginManager::cPluginManager(const char *Directory)
{
  directory = NULL;
  lastHousekeeping = time(NULL);
  nextHousekeeping = -1;
  if (pluginManager) {
     fprintf(stderr, "vdr: attempt to create more than one plugin manager - exiting!\n");
     exit(2);
     }
  SetDirectory(Directory);
  pluginManager = this;
}

cPluginManager::~cPluginManager()
{
  Shutdown();
  free(directory);
  if (pluginManager == this)
     pluginManager = NULL;
}

void cPluginManager::SetDirectory(const char *Directory)
{
  free(directory);
  directory = Directory ? strdup(Directory) : NULL;
}

void cPluginManager::AddPlugin(const char *Args)
{
  if (strcmp(Args, "*") == 0) {
     cFileNameList Files(directory);
     for (int i = 0; i < Files.Size(); i++) {
         char *FileName = Files.At(i);
         if (strstr(FileName, LIBVDR_PREFIX) == FileName) {
            char *p = strstr(FileName, SO_INDICATOR);
            if (p) {
               *p = 0;
               p += strlen(SO_INDICATOR);
               if (strcmp(p, APIVERSION) == 0) {
                  char *name = FileName + strlen(LIBVDR_PREFIX);
                  if (strcmp(name, "*") != 0) { // let's not get into a loop!
                     AddPlugin(FileName + strlen(LIBVDR_PREFIX));
                     }
                  }
               }
            }
         }
     return;
     }
  char *s = strdup(skipspace(Args));
  char *p = strchr(s, ' ');
  if (p)
     *p = 0;
  dlls.Add(new cDll(cString::sprintf("%s/%s%s%s%s", directory, LIBVDR_PREFIX, s, SO_INDICATOR, APIVERSION), Args));
  free(s);
}

bool cPluginManager::LoadPlugins(bool Log)
{
  for (cDll *dll = dlls.First(); dll; dll = dlls.Next(dll)) {
      if (!dll->Load(Log))
         return false;
      }
  return true;
}

bool cPluginManager::InitializePlugins(void)
{
  for (cDll *dll = dlls.First(); dll; dll = dlls.Next(dll)) {
      cPlugin *p = dll->Plugin();
      if (p) {
         isyslog("initializing plugin: %s (%s): %s", p->Name(), p->Version(), p->Description());
         if (!p->Initialize())
            return false;
         }
      }
  return true;
}

bool cPluginManager::StartPlugins(void)
{
  for (cDll *dll = dlls.First(); dll; dll = dlls.Next(dll)) {
      cPlugin *p = dll->Plugin();
      if (p) {
         isyslog("starting plugin: %s", p->Name());
         if (!p->Start())
            return false;
         p->started = true;
         }
      }
  return true;
}

void cPluginManager::Housekeeping(void)
{
  if (time(NULL) - lastHousekeeping > HOUSEKEEPINGDELTA) {
     if (++nextHousekeeping >= dlls.Count())
        nextHousekeeping = 0;
     cDll *dll = dlls.Get(nextHousekeeping);
     if (dll) {
        cPlugin *p = dll->Plugin();
        if (p) {
           p->Housekeeping();
           }
        }
     lastHousekeeping = time(NULL);
     }
}

void cPluginManager::MainThreadHook(void)
{
  for (cDll *dll = pluginManager->dlls.First(); dll; dll = pluginManager->dlls.Next(dll)) {
      cPlugin *p = dll->Plugin();
      if (p)
         p->MainThreadHook();
      }
}

bool cPluginManager::Active(const char *Prompt)
{
  if (pluginManager) {
     for (cDll *dll = pluginManager->dlls.First(); dll; dll = pluginManager->dlls.Next(dll)) {
         cPlugin *p = dll->Plugin();
         if (p) {
            cString s = p->Active();
            if (!isempty(*s)) {
               if (!Prompt || !Interface->Confirm(cString::sprintf("%s - %s", *s, Prompt)))
                  return true;
               }
            }
         }
     }
  return false;
}

cPlugin *cPluginManager::GetNextWakeupPlugin(void)
{
  cPlugin *NextPlugin = NULL;
  if (pluginManager) {
     time_t Now = time(NULL);
     time_t Next = 0;
     for (cDll *dll = pluginManager->dlls.First(); dll; dll = pluginManager->dlls.Next(dll)) {
         cPlugin *p = dll->Plugin();
         if (p) {
            time_t t = p->WakeupTime();
            if (t > Now && (!Next || t < Next)) {
               Next = t;
               NextPlugin = p;
               }
            }
         }
     }
  return NextPlugin;
}

bool cPluginManager::HasPlugins(void)
{
  return pluginManager && pluginManager->dlls.Count();
}

cPlugin *cPluginManager::GetPlugin(int Index)
{
  cDll *dll = pluginManager ? pluginManager->dlls.Get(Index) : NULL;
  return dll ? dll->Plugin() : NULL;
}

cPlugin *cPluginManager::GetPlugin(const char *Name)
{
  if (pluginManager && Name) {
     for (cDll *dll = pluginManager->dlls.First(); dll; dll = pluginManager->dlls.Next(dll)) {
         cPlugin *p = dll->Plugin();
         if (p && strcmp(p->Name(), Name) == 0)
            return p;
         }
     }
  return NULL;
}

cPlugin *cPluginManager::CallFirstService(const char *Id, void *Data)
{
  if (pluginManager) {
     for (cDll *dll = pluginManager->dlls.First(); dll; dll = pluginManager->dlls.Next(dll)) {
         cPlugin *p = dll->Plugin();
         if (p && p->Service(Id, Data))
            return p;
         }
     }
  return NULL;
}

bool cPluginManager::CallAllServices(const char *Id, void *Data)
{
  bool found=false;
  if (pluginManager) {
     for (cDll *dll = pluginManager->dlls.First(); dll; dll = pluginManager->dlls.Next(dll)) {
         cPlugin *p = dll->Plugin();
         if (p && p->Service(Id, Data))
            found = true;
         }
     }
  return found;
}

void cPluginManager::StopPlugins(void)
{
  for (cDll *dll = dlls.Last(); dll; dll = dlls.Prev(dll)) {
      cPlugin *p = dll->Plugin();
      if (p && p->started) {
         isyslog("stopping plugin: %s", p->Name());
         p->Stop();
         p->started = false;
         }
      }
}

void cPluginManager::Shutdown(bool Log)
{
  cDll *dll;
  while ((dll = dlls.Last()) != NULL) {
        cPlugin *p = dll->Plugin();
        if (p && Log)
           isyslog("deleting plugin: %s", p->Name());
        dlls.Del(dll);
        }
}
