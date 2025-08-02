/*
  -----------------------------------------------------------------------------
  secrets.h — Personal configuration for the Anker Solix monitor

  Copy this file to ``secrets.h`` and fill in your personal settings before
  compiling.  Do NOT commit your passwords or tokens to a public repository.

  Wi‑Fi credentials:
    WIFI_SSID       – name of your Wi‑Fi network
    WIFI_PASSWORD   – passphrase for your Wi‑Fi network

  Anker cloud configuration (used in MODE_ANKER_CLOUD):
    ANKER_USER      – e‑mail address or username used to sign in to Anker
    ANKER_PASSWORD  – password for your Anker account
    ANKER_COUNTRY   – ISO 3166‑1 country code used in your Anker profile (e.g. "DE")
    ANKER_AUTH_URL  – URL of the authentication endpoint.  Example:
                      "https://anker.tuyaeu.com/v1/user/login"
    ANKER_ENERGY_URL – URL to query daily energy data.  This endpoint must
                       return JSON containing the fields described in
                       ``fetchAnkerData``.

  Local smart‑meter configuration (used in MODE_LOCAL_SMARTMETER):
    SMARTMETER_HOST            – IP address or hostname of the smart‑meter on
                                 your LAN, e.g. "192.168.0.50"
    SMARTMETER_ENERGY_ENDPOINT – path of the API call returning daily energy
                                 data, starting with '/'.  Example: "/api/daily"
    SMARTMETER_TOKEN           – optional bearer token for authenticating with
                                 your smart‑meter.  Leave blank if not needed.

  All strings must be enclosed in double quotes.  For unused fields leave the
  string empty ("").
  -----------------------------------------------------------------------------
*/

#pragma once

// Wi‑Fi configuration
#define WIFI_SSID "fill here"
#define WIFI_PASSWORD "fill here"

// Anker cloud configuration
#define ANKER_USER "your_anker_username"
#define ANKER_PASSWORD "your_anker_password"
#define ANKER_COUNTRY "DE"

// The authentication URL and energy query URL must be provided by Anker.
#define ANKER_AUTH_URL ""
#define ANKER_ENERGY_URL ""

// Local smart‑meter configuration
#define SMARTMETER_HOST ""
#define SMARTMETER_ENERGY_ENDPOINT ""
#define SMARTMETER_TOKEN ""