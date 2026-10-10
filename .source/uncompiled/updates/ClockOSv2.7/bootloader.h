#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClientSecure.h>
#include <SD.h>

namespace BL {

enum Result { UP_TO_DATE, INSTALLED, FAILED };
using Progress = void (*)(int);

struct Candidate {
  String path;
  long version = -1;
  bool found = false;
};

inline long versionNumber(const String& value) {
  int v = value.lastIndexOf('V');
  if (v < 0) v = value.lastIndexOf('v');
  if (v < 0) return -1;
  String digits;
  for (int i = v + 1; i < value.length(); ++i) {
    char c = value[i];
    if ((c >= '0' && c <= '9') || c == '.') digits += c;
    else if (digits.length()) break;
  }
  if (!digits.length()) return -1;
  // Compare semantic components, not floating-point versions (2.10 must be
  // newer than 2.9). Reserve three decimal places per component.
  int firstDot = digits.indexOf('.');
  long major = firstDot < 0 ? digits.toInt() : digits.substring(0, firstDot).toInt();
  long minor = 0, patch = 0;
  if (firstDot >= 0) {
    int secondDot = digits.indexOf('.', firstDot + 1);
    minor = secondDot < 0 ? digits.substring(firstDot + 1).toInt()
                          : digits.substring(firstDot + 1, secondDot).toInt();
    if (secondDot >= 0) patch = digits.substring(secondDot + 1).toInt();
  }
  return major * 1000000L + constrain(minor, 0L, 999L) * 1000L + constrain(patch, 0L, 999L);
}

inline String installedPath() {
  if (!SD.exists(INSTALLED_FILE)) return String();
  File f = SD.open(INSTALLED_FILE, FILE_READ);
  if (!f) return String();
  String value = f.readStringUntil('\n');
  f.close();
  value.trim();
  return value;
}

inline void saveInstalledPath(const String& path) {
  SD.mkdir("/.source");
  SD.mkdir(DATA_DIR);
  SD.remove(INSTALLED_FILE);
  File f = SD.open(INSTALLED_FILE, FILE_WRITE);
  if (!f) return;
  f.println(path);
  f.close();
}

inline bool isCandidate(const String& path) {
  String prefix = String(GH_UPDATES_DIR) + "/";
  return path.startsWith(prefix) && path.endsWith(".bin");
}

inline Candidate newestCandidate(JsonArray tree) {
  Candidate best;
  for (JsonObject item : tree) {
    if (String((const char*)item["type"]) != "blob") continue;
    String path = item["path"] | "";
    if (!isCandidate(path)) continue;
    long version = versionNumber(path);
    if (!best.found || version > best.version || (version == best.version && path > best.path)) {
      best.path = path;
      best.version = version;
      best.found = true;
    }
  }
  return best;
}

inline Candidate findNewest() {
  Candidate none;
  WiFiClientSecure client;
  client.setInsecure(); // GitHub is HTTPS; certificate pinning can be added later.
  HTTPClient http;
  String url = "https://api.github.com/repos/" GH_OWNER "/" GH_REPO "/git/trees/main?recursive=1";
  if (!http.begin(client, url)) return none;
  http.addHeader("Accept", "application/vnd.github+json");
  http.addHeader("User-Agent", "Clock-ESP32");
  if (String(GH_TOKEN).length()) http.addHeader("Authorization", "Bearer " GH_TOKEN);
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return none;
  }
  DynamicJsonDocument doc(32768);
  DeserializationError error = deserializeJson(doc, http.getStream());
  Candidate result = error ? none : newestCandidate(doc["tree"].as<JsonArray>());
  http.end();
  return result;
}

inline Result installCandidate(const Candidate& candidate, Progress progress) {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://raw.githubusercontent.com/" GH_OWNER "/" GH_REPO "/main/" + candidate.path;
  if (!http.begin(client, url)) return FAILED;
  http.addHeader("User-Agent", "Clock-ESP32");
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return FAILED;
  }

  int total = http.getSize();
  if (!Update.begin(total > 0 ? (size_t)total : UPDATE_SIZE_UNKNOWN)) {
    http.end();
    return FAILED;
  }
  Stream* stream = http.getStreamPtr();
  size_t written = Update.writeStream(*stream);
  bool complete = total <= 0 || written == (size_t)total;
  if (progress && total > 0) progress((int)((written * 100UL) / (size_t)total));
  bool ok = complete && Update.end(true) && Update.isFinished();
  http.end();
  if (!ok) {
    Update.abort();
    return FAILED;
  }
  saveInstalledPath(candidate.path);
  if (progress) progress(100);
  return INSTALLED;
}

inline Result checkAndInstall(Progress progress = nullptr) {
  if (WiFi.status() != WL_CONNECTED) return FAILED;
  Candidate candidate = findNewest();
  if (!candidate.found) return UP_TO_DATE;
  String installed = installedPath();
  if (installed.length()) {
    long installedVersion = versionNumber(installed);
    if (candidate.version < installedVersion ||
        (candidate.version == installedVersion &&
         (candidate.path == installed || candidate.path < installed))) {
      return UP_TO_DATE;
    }
  }
  // Update.begin writes the inactive OTA slot. If any step fails, abort leaves
  // the currently running application intact; reboot occurs only after success.
  return installCandidate(candidate, progress);
}

inline bool run() {
  return checkAndInstall() == INSTALLED;
}

} // namespace BL
