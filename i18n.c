/*
 * i18n.c: Internationalization
 *
 * See the main source file 'vdr.c' for copyright information and
 * how to reach the author.
 *
 * $Id: i18n.c 1.56 2002/02/24 11:53:44 kls Exp $
 *
 * Slovenian translations provided by Miha Setina <mihasetina@softhome.net>
 * Italian   translations provided by Alberto Carraro <bertocar@tin.it>
 * Dutch     translations provided by Arnold Niessen <niessen@iae.nl> <arnold.niessen@philips.com>
 * Portugese translations provided by Paulo Manuel Martins Lopes <pmml@netvita.pt>
 * French    translations provided by Jean-Claude Repetto <jc@repetto.org>
 * Norwegian translations provided by Jørgen Tvedt <pjtvedt@online.no>
 * Finnish   translations provided by Hannu Savolainen <hannu@opensound.com>
 *
 */

/*
 * How to add a new language:
 *
 * 1. Announce your translation action on the Linux-DVB mailing
 *    list to avoid duplicate work.
 * 2. Increase the value of 'NumLanguages'.
 * 3. Insert a new line in every member of the 'Phrases[]' array,
 *    containing the translated text for the new language.
 *    For example, assuming you want to add the Italian language,
 *
 *       { "English",
 *         "Deutsch",
 *       },
 *
 *    would become
 *
 *       { "English",
 *         "Deutsch",
 *         "Italiano",
 *       },
 *
 *    and so on. Append your language after the last existing language
 *    and write the name of your language in your language (not in English,
 *    which means that it should be 'Italiano', not 'Italian').
 *    Note that only the characters defined in 'fontosd.c' will
 *    be available!
 * 4. Compile VDR and test the new language by switching to it
 *    in the "Setup" menu.
 * 5. Send the modified 'i18n.c' file to <kls@cadsoft.de> to have
 *    it included in the next version of VDR.
 */

#include "i18n.h"
#include <stdio.h>
#include "config.h"
#include "tools.h"

const int NumLanguages = 9;

typedef const char *tPhrase[NumLanguages];

const tPhrase Phrases[] = {
  // The name of the language (this MUST be the first phrase!):
  { "English",
    "Deutsch",
    "Slovenski",
    "Italiano",
    "Nederlands",
    "Portugues",
    "Français",
    "Norsk",
    "Suomi",
  },
  // Menu titles:
  { "Main",
    "Hauptmenü",
    "Glavni meni",
    "Principale",
    "Hoofdmenu",
    "Principal",
    "Menu",
    "Hovedmeny",
    "Valikko",
  },
  { "Schedule",
    "Programm",
    "Urnik",
    "Programmi",
    "Gids",
    "Programa",
    "Programmes",
    "Programmer",
    "Ohjelmat",
  },
  { "Channels",
    "Kanäle",
    "Kanali",
    "Canali",
    "Kanalen",
    "Canal",
    "Chaînes",
    "Kanaler",
    "Kanavat",
  },
  { "Timers",
    "Timer",
    "Termini",
    "Timer",
    "Timers",
    "Alarmes",
    "Programmation",
    "Timer",
    "Ajastin",
  },
  { "Recordings",
    "Aufzeichnungen",
    "Posnetki",
    "Registrazioni",
    "Opnames",
    "Gravacoes",
    "Enregistrements",
    "Opptak",
    "Nauhoitteet",
  },
  { "DVD",
    "DVD",
    "DVD",
    "DVD",
    "DVD",
    "DVD",
    "DVD",
    "DVD",
    "DVD",
  },
  { "Setup",
    "Einstellungen",
    "Nastavitve",
    "Opzioni",
    "Instellingen",
    "Configurar",
    "Configuration",
    "Konfigurasjon",
    "Asetukset",
  },
  { "Commands",
    "Befehle",
    "Ukazi",
    "Comandi",
    "Commando's",
    "Comandos",
    "Commandes",
    "Kommandoer",
    "Komennot",
  },
  { "Edit Channel",
    "Kanal Editieren",
    "Uredi kanal",
    "Modifica canale",
    "Kanaal aanpassen",
    "Modificar Canal",
    "Modifier une chaîne",
    "Editer Kanal",
    "Muokkaa kanavaa",
  },
  { "Edit Timer",
    "Timer Editieren",
    "Uredi termin",
    "Modifica Timer",
    "Timer veranderen",
    "Modificar Alarme",
    "Changer la programmation",
    "Editer Timer",
    "Muokkaa ajastusta",
  },
  { "Event",
    "Sendung",
    "Oddaja",
    "Eventi",
    "Uitzending",
    "Evento",
    "Evénement",
    "Hendelse",
    "Tapahtuma",
  },
  { "Summary",
    "Inhalt",
    "Vsebina",
    "Sommario",
    "Inhoud",
    "Resumo",
    "Résumé",
    "Sammendrag",
    "Yhteenveto",
  },
  { "Schedule - %s",
    "Programm - %s",
    "Urnik - %s",
    "Programma - %s",
    "Programma - %s",
    "Programa - %s",
    "Programmes - %s",
    "Program Guide - %s",
    "Ohjelma - %s",
  },
  { "What's on now?",
    "Was läuft jetzt?",
    "Kaj je na sporedu?",
    "In programmazione",
    "Wat is er nu?",
    "O que ver agora?",
    "Programmes en cours",
    "Hvilket program sendes nå?",
    "Nykyinen ohjelma",
  },
  { "What's on next?",
    "Was läuft als nächstes?",
    "Kaj sledi?",
    "Prossimi programmi",
    "Wat komt er hierna?",
    "O que ver depois?",
    "Prochains programmes",
    "Hvilket program er neste?",
    "Seuraava ohjelma",
  },
  // Button texts (should not be more than 10 characters!):
  { "Edit",
    "Editieren",
    "Uredi",
    "Modifica",
    "Verander",
    "Modificar",
    "Modifier",
    "Editer",
    "Muuta",
  },
  { "New",
    "Neu",
    "Novo",
    "Nuovo",
    "Nieuw",
    "Novo",
    "Nouveau",
    "Ny",
    "Uusi",
  },
  { "Delete",
    "Löschen",
    "Odstrani",
    "Cancella",
    "Verwijder",
    "Apagar",
    "Supprimer",
    "Slett",
    "Poista",
  },
  { "Mark",
    "Markieren",
    "Oznaci",
    "Marca",
    "Verplaats",
    "Marcar",
    "Marquer",
    "Marker",
    "Merkitse",
  },
  { "On/Off",
    "Ein/Aus",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
  },
  { "Record",
    "Aufnehmen",
    "Posnemi",
    "Registra",
    "Opnemen",
    "Gravar",
    "Enregistre",
    "Ta opp",
    "Nauhoita",
  },
  { "Play",
    "Wiedergabe",
    "Predavajaj",
    "Riproduci",
    "Afspelen",
    "Play",
    "Lire",
    "Spill av",
    "Toista",
  },
  { "Rewind",
    "Anfang",
    "Zacetek",
    "Da inizio",
    "Spoel terug",
    "Rebobinar",
    "Retour",
    "Spol tilbake",
    "Takaisinkel.",
  },
  { "Resume",
    "Weiter",
    "Nadaljuj",
    "Riprendi",
    "Verder",
    "Continuar",
    "Reprendre",
    "Fortsett",
    "Jatka",
  },
  { "Summary",
    "Inhalt",
    "Vsebina",
    "Sommario",
    "Inhoud",
    "Resumo",
    "Résumé",
    "Sammendrag",
    "Yhteenveto",
  },
  { "Open",
    "Öffnen",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Avaa",
  },
  { "Switch",
    "Umschalten",
    "Preklopi",
    "Cambia",
    "Selecteer",
    "Seleccionar",
    "Regarder",
    "Skift til",
    "Valitse",
  },
  { "Now",
    "Jetzt",
    "Sedaj",
    "Adesso",
    "Nu",
    "Agora",
    "Maintenant",
    "Nå",
    "Nyt",
  },
  { "Next",
    "Nächste",
    "Naslednji",
    "Prossimo",
    "Hierna",
    "Proximo",
    "Après",
    "Neste",
    "Seuraava",
  },
  { "Schedule",
    "Programm",
    "Urnik",
    "Programma",
    "Programma",
    "Programa",
    "Programme",
    "Programmer",
    "Ohjelmisto",
  },
  { "Language",
    "Sprache",
    "Jezik",
    "Linguaggio",
    "Taal",
    "", // TODO
    "Langue",
    "Språk",
    "Kieli",
  },
  { "Eject",
    "Auswerfen",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Ejection",
    "", // TODO
    "Avaa",
  },
  // Confirmations:
  { "Delete channel?",
    "Kanal löschen?",
    "Odstrani kanal?",
    "Cancello il canale?",
    "Kanaal verwijderen?",
    "Apagar o Canal?",
    "Supprimer la chaîne?",
    "Slette kanal?",
    "Poistetaanko kanava?",
  },
  { "Delete timer?",
    "Timer löschen?",
    "Odstani termin?",
    "Cancello il timer?",
    "Timer verwijderen?",
    "Apagar o Alarme?",
    "Supprimer la programmation?",
    "Slette timer?",
    "Poistetaanko ajastus?",
  },
  { "Delete recording?",
    "Aufzeichnung löschen?",
    "Odstrani posnetek?",
    "Cancello la registrazione?",
    "Opname verwijderen?",
    "Apagar Gravacão?",
    "Supprimer l'enregistrement?",
    "Slette opptak?",
    "Poistetaanko nauhoitus?",
  },
  { "Timer still recording - really delete?",
    "Timer zeichnet auf - trotzdem löschen?",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
  },
  { "Stop recording?",
    "Aufzeichnung beenden?",
    "Koncaj snemanje?",
    "Fermo la registrazione?",
    "Opname stoppen?",
    "Parar Gravacão?",
    "Arrêter l'enregistrement?",
    "Stoppe opptak?",
    "Pysäytetäänkö nauhoitus?",
  },
  { "on primary interface",
    "auf dem primären Interface",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "päävastaanottimella",
  },
  { "Cancel editing?",
    "Schneiden abbrechen?",
    "Zelite prekiniti urejanje?",
    "Annullo la modifica?",
    "Bewerken afbreken?",
    "Cancelar Modificar?",
    "Annuler les modifications?",
    "Avbryte redigering?",
    "Peruutetaanko muokkaus?",
  },
  { "Recording - shut down anyway?",
    "Aufnahme läuft - trotzdem ausschalten?",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Nauhoitus kesken - lopetetaanko se?",
  },
  { "Recording in %d minutes, shut down anyway?",
    "Aufnahme in %d Minuten - trotzdem ausschalten?",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
  },
  { "Press any key to cancel shutdown",
    "Taste drücken um Shutdown abzubrechen",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Peruuta pysäytys painamalla jotakin näppäintä",
  },
  // Channel parameters:
  { "Name",
    "Name",
    "Naziv",
    "Nome",
    "Naam",
    "Nome",
    "Nom",
    "Navn",
    "Nimi",
  },
  { "Frequency",
    "Frequenz",
    "Frekvenca",
    "Frequenza",
    "Frequentie",
    "Frequencia",
    "Fréquence",
    "Frekvens",
    "Taajuus",
  },
  { "Polarization",
    "Polarisation",
    "Polarizacija",
    "Polarizzazione",
    "Polarisatie",
    "Polarizacao",
    "Polarisation",
    "Polaritet",
    "Polarisaatio",
  },
  { "DiSEqC",
    "DiSEqC",
    "DiSEqC",
    "DiSEqC",
    "DiSEqC",
    "DiSEqC",
    "DiSEqC",
    "DiSEqC",
    "DiSEqC",
  },
  { "Srate",
    "Srate",
    "Srate",
    "Srate",
    "Srate",
    "Srate",
    "Fréq. Symbole",
    "Symbolrate",
    "Srate",
  },
  { "Vpid",
    "Vpid",
    "Vpid",
    "Vpid",
    "Vpid",
    "Vpid",
    "PID Vidéo",
    "Video pid",
    "Kuva PID",
  },
  { "Apid1",
    "Apid1",
    "Apid1",
    "Apid1",
    "Apid1",
    "Apid1",
    "PID Audio (1)",
    "Audio pid1",
    "Ääni PID1",
  },
  { "Apid2",
    "Apid2",
    "Apid2",
    "Apid2",
    "Apid2",
    "Apid2",
    "PID Audio (2)",
    "Audio pid2",
    "Ääni PID2",
  },
  { "Dpid1",
    "Dpid1",
    "Dpid1",
    "Dpid1",
    "Dpid1",
    "Dpid1",
    "PID AC3 (1)",
    "AC3 pid1",
    "AC3 PID1",
  },
  { "Dpid2",
    "Dpid2",
    "Dpid2",
    "Dpid2",
    "Dpid2",
    "Dpid2",
    "PID AC3 (2)",
    "AC3 pid2",
    "AC3 PID2",
  },
  { "Tpid",
    "Tpid",
    "Tpid",
    "Tpid",
    "Tpid",
    "Tpid",
    "PID Télétexte",
    "Teletext pid",
    "TekstiTV PID",
  },
  { "CA",
    "CA",
    "CA",
    "CA",
    "CA",
    "CA",
    "Cryptage",
    "Kortleser",
    "Salauskortti",
  },
  { "Pnr",
    "Pnr",
    "Pnr",
    "Pnr",
    "Pnr",
    "Pnr",
    "Num. Progr.",
    "Program Id",
    "Ohjelmatunnus",
  },
  // Timer parameters:
  { "Active",
    "Aktiv",
    "Aktivno",
    "Attivo",
    "Actief",
    "Activo",
    "Actif",
    "Aktiv",
    "Aktiivinen",
  },
  { "Channel",
    "Kanal",
    "Kanal",
    "Canale",
    "Kanaal",
    "Canal",
    "Chaîne",
    "Kanal",
    "Kanava",
  },
  { "Day",
    "Tag",
    "Dan",
    "Giorno",
    "Dag",
    "Dia",
    "Jour",
    "Dag",
    "Päivä",
  },
  { "Start",
    "Anfang",
    "Zacetek",
    "Inizio",
    "Begin",
    "Inicio",
    "Début",
    "Start",
    "Aloitus",
  },
  { "Stop",
    "Ende",
    "Konec",
    "Fine",
    "Einde",
    "Fim",
    "Fin",
    "Slutt",
    "Lopetus",
  },
  { "Priority",
    "Priorität",
    "Prioriteta",
    "Priorita",
    "Prioriteit",
    "Prioridade",
    "Priorité",
    "Prioritet",
    "Prioriteetti",
  },
  { "Lifetime",
    "Lebensdauer",
    "Veljavnost",
    "Durata",
    "Bewaarduur",
    "Duracao",
    "Durée de vie",
    "Levetid",
    "Voimassaolo",
  },
  { "File",
    "Datei",
    "Datoteka",
    "Nome",
    "Filenaam",
    "Ficheiro",
    "Fichier",
    "Filnavn",
    "Tiedosto",
  },
  { "First day",
    "Erster Tag",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
  },
  // Error messages:
  { "Channel is being used by a timer!",
    "Kanal wird von einem Timer benutzt!",
    "Urnik zaseda kanal!",
    "Canale occupato da un timer!",
    "Kanaal wordt gebruikt door een timer!",
    "Canal a ser utilizador por um alarme!",
    "Cette chaîne est en cours d'utilisation!",
    "Kanalen er i bruk av en timer!",
    "Kanava on ajastimen käytössä!",
  },
  { "Can't switch channel!",
    "Kanal kann nicht umgeschaltet werden!",
    "Ne morem preklopiti kanala!",
    "Impossibile cambiare canale!",
    "Kan geen kanaal wisselen!",
    "Nao pode mudar de canal!",
    "Impossible de changer de chaîne!",
    "Ikke mulig å skifte kanal!",
    "Kanavan vaihtaminen ei mahdollista!",
  },
  { "Timer is recording!",
    "Timer zeichnet gerade auf!",
    "Snemanje po urniku!",
    "Registrazione di un timer in corso!",
    "Timer is aan het opnemen!",
    "Alarme a gravar!",
    "Enregistrement en cours!",
    "Timer gjør opptak!",
    "Ajastinnauhoitus käynnissä!",
  },
  { "Error while accessing recording!",
    "Fehler beim ansprechen der Aufzeichnung!",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Nauhoituksen toistaminen epäonnistui!",
  },
  { "Error while deleting recording!",
    "Fehler beim Löschen der Aufzeichnung!",
    "Napaka pri odstranjevanju posnetka!",
    "Errore durante la canc del filmato!",
    "Fout bij verwijderen opname!",
    "Erro enquanto apagava uma gravacao!",
    "Erreur de suppression de l'enregistrement!",
    "Feil under sletting av opptak!",
    "Nauhoituksen poistaminen epäonnistui!",
  },
  { "*** Invalid Channel ***",
    "*** Ungültiger Kanal ***",
    "*** Neznan kanal ***",
    "*** CANALE INVALIDO ***",
    "*** Ongeldig kanaal ***",
    "*** Canal Invalido! ***",
    "*** Chaîne invalide! ***",
    "*** Ugyldig Kanal! ***",
    "*** Virheellinen kanavavalinta! ***",
  },
  { "No free DVB device to record!",
    "Keine freie DVB-Karte zum Aufnehmen!",
    "Ni proste DVB naprave za snemanje!",
    "Nessuna card DVB disp per registrare!",
    "Geen vrije DVB kaart om op te nemen!",
    "Nenhuma placa DVB disponivel para gravar!",
    "Pas de carte DVB disponible pour l'enregistrement!",
    "Ingen ledige DVB enheter for opptak!",
    "Ei vapaata vastaanotinta nauhoitusta varten!",
  },
  { "Channel locked (recording)!",
    "Kanal blockiert (zeichnet auf)!",
    "Zaklenjen kanal (snemanje)!",
    "Canale bloccato (in registrazione)!",
    "Kanaal geblokkeerd (neemt op)!",
    "Canal bloqueado (a gravar)!",
    "Chaîne verrouillée (enregistrement en cours)!",
    "Kanalen er låst (opptak)!",
    "Kanava lukittu (nauhoitusta varten)!",
  },
  { "Can't start Transfer Mode!",
    "Transfer-Mode kann nicht gestartet werden!",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Käsittämättömiä teknisiä ongelmia!",
  },
  { "Can't start editing process!",
    "Schnitt kann nicht gestartet werden!",
    "Ne morem zaceti urejanja!",
    "Imposs iniziare processo di modifica",
    "Kan niet beginnen met bewerken!",
    "Nao pode iniciar a modificacao!",
    "Impossible de commencer le montage!",
    "Kan ikke starte redigeringsprosessen!",
    "Muokkauksen aloittaminen ei onnistu!",
  },
  { "Editing process already active!",
    "Schnitt bereits aktiv!",
    "Urejanje je ze aktivno!",
    "Processo di modifica gia` attivo",
    "Bewerken is al actief!",
    "Processo de modificacao ja activo!",
    "Montage déjà en cours!",
    "Redigeringsprosessen er allerede aktiv!",
    "Muokkaus on jo käynnissä!",
  },
  { "Can't shutdown - option '-s' not given!",
    "Shutdown unmöglich - Option '-s' fehlt!",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Ei voida sammuttaa '-s' parametria ei annettu!",
  },
  { "Low disk space!",
    "Platte beinahe voll!",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Kovalevy lähes täynnä!",
  },
  // Setup parameters:
  { "OSD-Language",
    "OSD-Sprache",
    "OSD-jezik",
    "Linguaggio OSD",
    "OSD-taal",
    "Linguagem OSD",
    "Langue OSD",
    "OSD Språk",
    "Näytön kieli",
  },
  { "PrimaryDVB",
    "Primäres Interface",
    "Primarna naprava",
    "Scheda DVB primaria",
    "Eerste DVB kaart",
    "DVB primario",
    "Première carte DVB",
    "Hoved DVB-enhet",
    "Ensisij. vast.otin",
  },
  { "ShowInfoOnChSwitch",
    "Info zeigen",
    "Pokazi naziv kanala",
    "Vis info nel cambio canale",
    "Kanaal info tonen",
    "Mostrar info ao mudar de Canal",
    "Affichage progr. en cours",
    "Info ved kanalskifte",
    "Näytä kanavainfo",
  },
  { "MenuScrollPage",
    "Seitenweise scrollen",
    "Drsni meni",
    "Scrolla pagina nel menu",
    "Scrollen per pagina",
    "Scroll da pagina no menu",
    "Affichage progr. suivant",
    "Rask rulling i menyer",
    "Valikkojen rullaus",
  },
  { "MarkInstantRecord",
    "Direktaufz. markieren",
    "Oznaci direktno snemanje",
    "Marca la registrazione",
    "Direkte opnamen markeren",
    "Marca de gravacao",
    "Enregistrement immédiat",
    "Markere direkteopptak",
    "Merkitse välitön nauh.",
  },
  { "NameInstantRecord",
    "Direktaufz. benennen",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Nimeä välitön nauh.",
  },
  { "LnbSLOF",
    "LnbSLOF",
    "LnbSLOF",
    "LnbSLOF",
    "LnbSLOF",
    "LnbSLOF",
    "Limite de bandes LNB",
    "LO-grensefrekvens",
    "LnbSLOF",
  },
  { "LnbFrequLo",
    "Untere LNB-Frequenz",
    "Spodnja LNB-frek.",
    "Freq LO LNB",
    "Laagste LNB frequentie",
    "Freq LO LNB",
    "Fréquence basse LNB",
    "LO-frekvens i lavbåndet",
    "LO LNB taajuus",
  },
  { "LnbFrequHi",
    "Obere LNB-Frequenz",
    "Zgornja LNB-frek.",
    "Freq HI LNB",
    "Hoogste LNB frequentie",
    "Freq HI LNB",
    "Fréquence haute LNB",
    "LO-frekvens i høybåndet",
    "HI LNB taajuus",
  },
  { "DiSEqC",
    "DiSEqC",
    "DiSEqC",
    "DiSEqC",
    "DiSEqC",
    "DiSEqC",
    "DiSEqC",
    "DiSEqC",
    "DiSEqC",
  },
  { "SetSystemTime",
    "Systemzeit stellen",
    "Sistemski cas",
    "Setta orario auto",
    "Systeem klok instellen",
    "Ajustar relogio do sistema",
    "Ajuster l'heure du système",
    "Juster system-klokken",
    "Vastaanota kellonaika",
  },
  { "MarginStart",
    "Zeitpuffer bei Anfang",
    "Premor pred zacetkom",
    "Min margine inizio",
    "Tijd marge (begin)",
    "Margem de inicio",
    "Marge antérieure",
    "Opptaks margin (start)",
    "Aloitusmarginaali",
  },
  { "MarginStop",
    "Zeitpuffer bei Ende",
    "Premor za koncem",
    "Min margine fine",
    "Tijd marge (eind)",
    "Margem de fim",
    "Marge postérieure",
    "Opptaks margin (slutt)",
    "Lopetusmarginaali",
  },
  { "EPGScanTimeout",
    "Zeit bis EPG Scan",
    "Cas do EPG pregleda",
    "Timeout EPG",
    "EPG-scan Timeout",
    "Timeout EPG",
    "Temps maxi EPG",
    "Ledig tid før EPG-søk",
    "Ohjelmatied. odotusaika",
  },
  { "EPGBugfixLevel",
    "EPG Fehlerbereinigung",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "EPGBugfixLevel",
  },
  { "SVDRPTimeout",
    "SVDRP Timeout",
    "", // TODO
    "Timeout SVDRP",
    "SVDRP Timeout",
    "Timeout SVDRP",
    "Temps maxi SVDRP",
    "Ubrukt SVDRP-levetid",
    "SVDRP odotusaika",
  },
  { "SortTimers",
    "Timer sortieren",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Järjestä ajastimet",
  },
  { "PrimaryLimit",
    "Primär-Limit",
    "", // TODO
    "", // TODO
    "", // TODO
    "Limite Primario",
    "Première limite",
    "Prioritets grense HovedDVB",
    "PrimaryLimit",
  },
  { "DefaultPriority",
    "Default Priorität",
    "", // TODO
    "", // TODO
    "", // TODO
    "Prioridade por defeito",
    "Priorité par défaut",
    "Normal prioritet (Timer)",
    "Oletusprioriteetti",
  },
  { "DefaultLifetime",
    "Default Lebensdauer",
    "", // TODO
    "", // TODO
    "", // TODO
    "Validade por defeito",
    "Durée de vie par défaut",
    "Normal levetid (Timer)",
    "Oletus voimassaoloaika",
  },
  { "UseSubtitle",
    "Subtitle verwenden",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Tekstitys käytössä",
  },
  { "RecordingDirs",
    "Aufn. Verzeichnisse",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Nauhoitushakemistot",
  },
  { "VideoFormat",
    "Video Format",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Format vidéo",
    "TV Format",
    "Kuvamuoto",
  },
  { "RecordDolbyDigital",
    "Dolby Digital Ton aufz.",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
  },
  { "ChannelInfoPos",
    "Kanal Info Position",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Position infos chaînes",
    "", // TODO
    "Kanavainfon sijainti",
  },
  { "OSDwidth",
    "OSD Breite",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Largeur affichage",
    "", // TODO
    "Tekstinäytön leveys",
  },
  { "OSDheight",
    "OSD Höhe",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Hauteur affichage",
    "", // TODO
    "Tekstinäytön korkeus",
  },
  { "OSDMessageTime",
    "OSD Nachricht Dauer",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Ilmoitusten näkymisaika",
  },
  { "MaxVideoFileSize",
    "Max. Video Dateigröße",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Maksimi tiedoston koko",
  },
  { "SplitEditedFiles",
    "Editierte Dateien zerteilen",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Paloittele muokatut",
  },
  { "MinEventTimeout",
    "Mindest Event Pause",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Minimi tapahtuman odotus",
  },
  { "MinUserInactivity",
    "Mindest User Inaktivität",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Minimi käyttäjän odotus",
  },
  { "MultiSpeedMode",
    "MultiSpeed Modus",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Moninopeustila",
  },
  { "ShowReplayMode",
    "Wiedergabe Status",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Näytä toiston tila",
  },
  // The days of the week:
  { "MTWTFSS",
    "MDMDFSS",
    "PTSCPSN",
    "DLMMGVS",
    "MDWDVZZ",
    "STQQSSD",
    "LMMJVSD",
    "MTOTFLS",
    "MTKTPLS",
  },
  { "MonTueWedThuFriSatSun", // must all be 3 letters!
    "MonDieMitDonFreSamSon",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "MaaTiiKesTorPerLauSun",
  },
  // Learning keys:
  { "Learning Remote Control Keys",
    "Fernbedienungs-Codes lernen",
    "Ucim se kod upravljalca",
    "Apprendimento tasti unita` remota",
    "Leren toetsen afstandsbediening",
    "Aprender as teclas do telecomando",
    "Apprentissage des codes de télécommande",
    "Lære fjernkontrolltaster",
    "Kaukosäätimen näppäinten opettelu",
  },
  { "Phase 1: Detecting RC code type",
    "Phase 1: FB Code feststellen",
    "Faza 1: Sprejemanje IR kode",
    "Fase 1: tipo ricevitore RC",
    "Fase 1: detecteren type afstandsbediening",
    "Fase 1: detectar tipo de receptor",
    "Phase 1: Détection du type de code",
    "Fase 1: Finne fjernkontroll-kodetype",
    "Vaihe 1: Lähetystavan selvittäminen",
  },
  { "Press any key on the RC unit",
    "Eine Taste auf der FB drücken",
    "Pritisnite tipko na upravljalcu",
    "Premere un tasto nell'unita` RC",
    "Druk op een willekeurige knop",
    "Pressione qualquer tecla do telecomando", 
    "Appuyer sur une touche de la télécommande",
    "Trykk en av tastene på fjernkontrollen",
    "Paina mitä tahansa kaukosäätimen näppäintä",
  },
  { "RC code detected!",
    "FB Code erkannt!",
    "IR koda sprejeta!",
    "Codice RC rilevato!",
    "Afstandsbediening code herkend!",
    "Codigo do telecomando detectado!",
    "Code de la télécommande détecté!",
    "Fjernkontroll-kodetype funnet!",
    "Näppäinpainallus vastaanotettu!",
  },
  { "Do not press any key...",
    "Keine Taste drücken...",
    "Ne pritiskajte tipk...",
    "Non premere alcun tasto...",
    "Druk niet op een knop...",
    "Nao pressione nada...",
    "Ne pas appuyer sur une touche ...",
    "Ikke trykk på noen av tastene...",
    "Älä paina mitään näppäintä...",
  },
  { "Phase 2: Learning specific key codes",
    "Phase 2: Einzelne Tastencodes lernen",
    "Faza 2: Ucenje posebnih kod",
    "Fase 2: Codici specifici dei tasti",
    "Fase 2: Leren specifieke toets-codes",
    "Fase 2: A aprender codigos especificos",
    "Phase 2: Apprentissage des codes des touches",
    "Fase 2: Lære spesifikke tastekoder",
    "Vaihe 2: Näppäinkoodien opettelu",
  },
  { "Press key for '%s'",
    "Taste für '%s' drücken",
    "Pritisnite tipko za '%s'",
    "Premere il tasto per '%s'",
    "Druk knop voor '%s'",
    "Pressione tecla para '%s'",
    "Appuyer sur la touche '%s'",
    "Trykk tasten for '%s'",
    "Paina näppäintä toiminnolle '%s'",
  },
  { "Press 'Up' to confirm",
    "'Auf' drücken zum Bestätigen",
    "Pritisnite tipko 'Gor' za potrditev",
    "Premere 'Su' per confermare",
    "Druk 'Omhoog' om te bevestigen",
    "Pressione 'Cima' para confirmar",
    "Appuyer sur 'Haut' pour confirmer",
    "Trykk 'Opp' for å bekrefte",
    "Paina 'Ylös' hyväksyäksesi",
  },
  { "Press 'Down' to continue",
    "'Ab' drücken zum Weitermachen",
    "Pritisnite tipko 'Dol' za nadaljevanje",
    "Premere 'Giu' per confermare",
    "Druk 'Omlaag' om verder te gaan",
    "Pressione 'Baixo' para continuar",
    "Appuyer sur 'Bas' pour continuer",
    "Trykk Ned' for å fortsette",
    "Paina 'Alas' jatkaaksesi",
  },
  { "(press 'Up' to go back)",
    "('Auf' drücken um zurückzugehen)",
    "(pritisnite 'Gor' za nazaj)",
    "(premere 'Su' per tornare indietro)",
    "(druk 'Omhoog' om terug te gaan)",
    "(Pressione 'Cima' para voltar)",
    "(Appuyer sur 'Haut' pour revenir en arrière)",
    "(trykk 'Opp' for å gå tilbake)",
    "(paina 'Ylös' palataksesi takaisin)",
  },
  { "(press 'Down' to end key definition)",
    "('Ab' drücken zum Beenden)",
    "(pritisnite 'Dol' za konec)",
    "('Giu' per finire la definiz tasti)",
    "(Druk 'Omlaag' om te beeindigen)",
    "(Pressione 'Baixo' para terminar a definicao)",
    "(Appuyer sur 'Bas' pour terminer)",
    "(trykk 'Ned' for å avslutte innlæring)",
    "(paina 'Alas' lopettaaksesi näppäinten opettelun)",
  },
  { "Phase 3: Saving key codes",
    "Phase 3: Codes abspeichern",
    "Faza 3: Shranjujem kodo",
    "Fase 3: Salvataggio key codes",
    "Fase 3: Opslaan toets codes",
    "Fase 3: A Salvar os codigos das teclas",
    "Phase 3: Sauvegarde des codes des touches",
    "Fase 3: Lagre tastekoder",
    "Vaihe 3: Näppäinkoodien tallettaminen",
  },
  { "Press 'Up' to save, 'Down' to cancel",
    "'Auf' speichert, 'Ab' bricht ab",
    "'Gor' za potrditev, 'Dol' za prekinitev",
    "'Su' per salvare, 'Giu' per annullare",
    "'Omhoog' te bewaren, 'Omlaag' voor annuleren",
    "'Cima' para Salvar, 'Baixo' para Cancelar",
    "Appuyer sur 'Haut' pour sauvegarder, 'Bas' pour annuler",
    "Trykk 'Opp' for å lagre, 'Ned' for å avbryte",
    "Paina 'Ylös' tallettaaksesi ja 'Alas' peruuttaaksesi",
  },
  // Key names:
  { "Up",
    "Auf",
    "Gor",
    "Su",
    "Omhoog",
    "Cima",
    "Haut",
    "Opp",
    "Ylös",
  },
  { "Down",
    "Ab",
    "Dol",
    "Giu",
    "Omlaag",
    "Baixo",
    "Bas",
    "Ned",
    "Alas",
  },
  { "Menu",
    "Menü",
    "Meni",
    "Menu",
    "Menu",
    "Menu",
    "Menu",
    "Meny",
    "Valikko",
  },
  { "Ok",
    "Ok",
    "Ok",
    "Ok",
    "Ok",
    "Ok",
    "Ok",
    "Ok",
    "Ok",
  },
  { "Back",
    "Zurück",
    "Nazaj",
    "Indietro",
    "Terug",
    "Voltar",
    "Retour",
    "Tilbake",
    "Takaisin",
  },
  { "Left",
    "Links",
    "Levo",
    "Sinistra",
    "Links",
    "Esquerda",
    "Gauche",
    "Venstre",
    "Vasemmalle",
  },
  { "Right",
    "Rechts",
    "Desno",
    "Destra",
    "Rechts",
    "Direita",
    "Droite",
    "Høyre",
    "Oikealle",
  },
  { "Red",
    "Rot",
    "Rdeca",
    "Rosso",
    "Rood",
    "Vermelho",
    "Rouge",
    "Rød",
    "Punainen",
  },
  { "Green",
    "Grün",
    "Zelena",
    "Verde",
    "Groen",
    "Verde",
    "Vert",
    "Grønn",
    "Vihreä",
  },
  { "Yellow",
    "Gelb",
    "Rumena",
    "Giallo",
    "Geel",
    "Amarelo",
    "Jaune",
    "Gul",
    "Keltainen",
  },
  { "Blue",
    "Blau",
    "Modra",
    "Blu",
    "Blauw",
    "Azul",
    "Bleu",
    "Blå",
    "Sininen",
  },
  { "Power",
    "Ausschalten",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Virtakytkin",
  },
  { "Volume+",
    "Lautstärke+",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Äänenvoimakkuus+",
  },
  { "Volume-",
    "Lautstärke-",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Äänenvoimakkuus-",
  },
  { "Mute",
    "Stumm",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Äänen vaimennus",
  },
  // Miscellaneous:
  { "yes",
    "ja",
    "da",
    "si",
    "ja",
    "sim",
    "oui",
    "ja",
    "kyllä",
  },
  { "no",
    "nein",
    "ne",
    "no",
    "nee",
    "nao",
    "non",
    "nei",
    "ei",
  },
  { "top",
    "oben",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "haut",
    "", // TODO
    "ylä",
  },
  { "bottom",
    "unten",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "bas",
    "", // TODO
    "ala",
  },
  { "free",
    "frei",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "vapaa",
  },
  { "Jump: ", // note the trailing blank
    "Springen: ",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Hyppää",
  },
  { " Stop replaying", // note the leading blank!
    " Wiedergabe beenden",
    " Prekini ponavljanje",
    " Interrompi riproduzione",
    " Stop afspelen",
    " Parar reproducao",
    " Arrêter la lecture",
    " Stopp avspilling",
    " Pysäytä toisto",
  },
  { " Stop recording ", // note the leading and trailing blanks!
    " Aufzeichnung beenden ",
    " Prekini shranjevanje ",
    " Interrompi registrazione ",
    " Stop opnemen ",
    " Parar gravacao ",
    " Arrêter l'enregistrement ",
    " Stopp opptak fra ",
    " Pysäytä nauhoitus ",
  },
  { " Cancel editing", // note the leading blank!
    " Schneiden abbrechen",
    " Prekini urejanje",
    " Annulla modifiche",
    " Bewerken afbreken",
    " Anular modificacao",
    " Annuler le montage",
    " Avbryt editering",
    " Peruuta muokkaus",
  },
  { "Switching primary DVB...",
    "Primäres Interface wird umgeschaltet...",
    "Preklapljanje primarne naprave...",
    "Cambio su card DVB primaria...",
    "Eerste DVB-kaart wordt omgeschakeld...",
    "A mudar placa DVB primaria...",
    "Changement de carte DVB principale...",
    "Bytter hoved DVB-enhet...",
    "Vaihdetaan ensisijainen vastaanotin...",
  },
  { "Up/Dn for new location - OK to move",
    "Auf/Ab für neue Position - dann OK",
    "Gor/Dol za novo poz. - Ok za premik",
    "Su/Giu per nuova posizione - OK per muovere",
    "Gebruik Omhoog/Omlaag - daarna Ok",
    "Cima/Baixo para nova localizacao - Ok para mudar",
    "Haut/Bas -> nouvelle place - OK -> déplacer",
    "Opp/Ned for ny plass - OK for å flytte",
    "Ylös/Alas = liiku, OK = siirrä",
  },
  { "Editing process started",
    "Schnitt gestartet",
    "Urejanje se je zacelo",
    "Processo di modifica iniziato",
    "Bewerken is gestart",
    "Processo de modificacao iniciado",
    "Opération de montage lancée",
    "Redigeringsprosess startet",
    "Muokkaus aloitettu",
  },
  { "Editing process finished",
    "Schnitt beendet",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Muokkaus lopetettu",
  },
  { "Editing process failed!",
    "Schnitt gescheitert!",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "Muokkaus epäonnistui",
  },
  { "scanning recordings...",
    "Aufzeichnungen werden durchsucht...",
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "", // TODO
    "haetaan nauhoituksia...",
  },
  { NULL }
  };

const char *tr(const char *s)
{
  if (Setup.OSDLanguage) {
     for (const tPhrase *p = Phrases; **p; p++) {
         if (strcmp(s, **p) == 0) {
            const char *t = (*p)[Setup.OSDLanguage];
            if (t && *t)
               return t;
            }
         }
     esyslog(LOG_ERR, "no translation found for '%s' in language %d (%s)\n", s, Setup.OSDLanguage, Phrases[0][Setup.OSDLanguage]);
     }
  return s;
}

const char * const * Languages(void)
{
  return &Phrases[0][0];
}

