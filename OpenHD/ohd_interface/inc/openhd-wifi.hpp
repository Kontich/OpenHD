#ifndef OPENHD_WIFI_H
#define OPENHD_WIFI_H

#include <string>
#include <fstream>

#include "json.hpp"
#include "openhd-util.hpp"
#include "openhd-log.hpp"
#include "openhd-settings.hpp"
#include "openhd-util-filesystem.hpp"

enum class WiFiCardType {
  Unknown = 0,
  Realtek8812au,
  Realtek8814au,
  Realtek88x2bu,
  Realtek8188eu,
  Atheros9khtc,
  Atheros9k,
  Ralink,
  Intel,
  Broadcom,
};
NLOHMANN_JSON_SERIALIZE_ENUM( WiFiCardType, {
   {WiFiCardType::Unknown, nullptr},
   {WiFiCardType::Realtek8188eu, "Realtek8188eu"},
   {WiFiCardType::Realtek8814au, "Realtek8814au"},
   {WiFiCardType::Realtek88x2bu, "Realtek88x2bu"},
   {WiFiCardType::Realtek8188eu, "Realtek8188eu"},
   {WiFiCardType::Atheros9khtc, "Atheros9khtc"},
   {WiFiCardType::Atheros9k, "Atheros9k"},
   {WiFiCardType::Ralink, "Ralink"},
   {WiFiCardType::Intel, "Intel"},
   {WiFiCardType::Broadcom, "Broadcom"},
});

static std::string wifi_card_type_to_string(const WiFiCardType &card_type) {
  switch (card_type) {
    case WiFiCardType::Atheros9k:  return "ath9k";
    case WiFiCardType::Atheros9khtc: return "ath9k_htc";
    case WiFiCardType::Realtek8812au: return "88xxau";
    case WiFiCardType::Realtek88x2bu: return "88x2bu";
    case WiFiCardType::Realtek8188eu: return "8188eu";
    case WiFiCardType::Ralink: return "rt2800usb";
    case WiFiCardType::Intel: return "iwlwifi";
    case WiFiCardType::Broadcom: return "brcmfmac";
    default: return "unknown";
  }
}

enum class WiFiHotspotType {
  None = 0,
  Internal2GBand,
  Internal5GBand,
  InternalDualBand,
  External,
};
static std::string wifi_hotspot_type_to_string(const WiFiHotspotType &wifi_hotspot_type) {
  switch (wifi_hotspot_type) {
    case WiFiHotspotType::Internal2GBand:return "internal2g";
    case WiFiHotspotType::Internal5GBand:  return "internal5g";
    case WiFiHotspotType::InternalDualBand:  return "internaldualband";
    case WiFiHotspotType::External:  return "external";
    case WiFiHotspotType::None:
    default:
      return "none";
  }
}

// What to use a discovered wifi card for. R.n We support hotspot or monitor mode (wifibroadcast),
// I doubt that will change.
enum class WifiUseFor {
  Unknown = 0, // Not sure what to use this wifi card for, aka unused.
  MonitorMode, //Use for wifibroadcast, aka set to monitor mode.
  Hotspot, //Use for hotspot, aka start a wifi hotspot with it.
};
NLOHMANN_JSON_SERIALIZE_ENUM( WifiUseFor, {
   {WifiUseFor::Unknown, nullptr},
   {WifiUseFor::MonitorMode, "MonitorMode"},
   {WifiUseFor::Hotspot, "Hotspot"},
});
static std::string wifi_use_for_to_string(const WifiUseFor wifi_use_for){
  switch (wifi_use_for) {
    case WifiUseFor::Hotspot:return "hotspot";
    case WifiUseFor::MonitorMode:return "monitor_mode";
    case WifiUseFor::Unknown:
    default:
      return "unknown";
  }
}

static constexpr auto DEFAULT_WIFI_TX_POWER="3100";

struct WifiCardSettings{
  // This one needs to be set for the card to then be used for something.Otherwise, it is not used for anything
  WifiUseFor use_for = WifiUseFor::Unknown;
  // frequency for this card
  std::string frequency;
  // transmission power for this card
  std::string txpower=DEFAULT_WIFI_TX_POWER;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(WifiCardSettings,use_for,frequency,txpower)

struct WiFiCard {
  std::string driver_name; // Name of the driver that runs this card.
  WiFiCardType type = WiFiCardType::Unknown; // Detected wifi card type, generated by checking known drivers.
  std::string interface_name;
  std::string mac;
  bool supports_5ghz = false;
  bool supports_2ghz = false;
  bool supports_injection = false;
  bool supports_hotspot = false;
  bool supports_rts = false;
  // These are values that can change dynamically at run time.
  WifiCardSettings settings;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(WiFiCard,driver_name,type,interface_name,mac,supports_5ghz,supports_2ghz,
                                   supports_injection,supports_hotspot,supports_rts,settings)

static const std::string WIFI_SETTINGS_DIRECTORY=std::string(BASE_PATH)+std::string("interface/");
// WifiCardHolder is used to
// 1) Differentiate between immutable information (like mac address) and
// 2) mutable WiFi card settings.
// Setting changes are propagated through this class.
class WifiCardHolder{
 public:
  explicit WifiCardHolder(WiFiCard wifi_card):_wifi_card(std::move(wifi_card)){
    if(!OHDFilesystemUtil::exists(WIFI_SETTINGS_DIRECTORY.c_str())){
      OHDFilesystemUtil::create_directory(WIFI_SETTINGS_DIRECTORY);
    }
    const auto last_settings_opt=read_last_settings();
    if(last_settings_opt.has_value()){
      _settings=std::make_unique<WifiCardSettings>(last_settings_opt.value());
      std::cout<<"Found settings in:"<<get_unique_filename()<<"\n";
    }else{
      std::cout<<"Creating default settings:"<<get_unique_filename()<<"\n";
      // create default settings and persist them for the next reboot
      _settings=std::make_unique<WifiCardSettings>();
      persist_settings();
    }
  }
  // delete copy and move constructor
  WifiCardHolder(const WifiCardHolder&)=delete;
  WifiCardHolder(const WifiCardHolder&&)=delete;
 public:
  const WiFiCard _wifi_card;
  [[nodiscard]] const WifiCardSettings& get_settings()const{
    assert(_settings);
    return *_settings;
  }
 private:
  std::unique_ptr<WifiCardSettings> _settings;
  [[nodiscard]] std::string get_uniqe_hash()const{
    std::stringstream ss;
    ss<<wifi_card_type_to_string(_wifi_card.type)<<"_"<<_wifi_card.mac;
    return ss.str();
  }
  [[nodiscard]] std::string get_unique_filename()const{
    return WIFI_SETTINGS_DIRECTORY+get_uniqe_hash();
  }
  // write settings locally for persistence
  void persist_settings()const{
    assert(_settings);
    const auto filename= get_unique_filename();
    const nlohmann::json tmp=*_settings;
    // and write them locally for persistence
    std::ofstream t(filename);
    t << tmp.dump(4);
    t.close();
  }
  // read last settings, if they are available
  [[nodiscard]] std::optional<WifiCardSettings> read_last_settings()const{
    const auto filename= get_unique_filename();
    if(!OHDFilesystemUtil::exists(filename.c_str())){
      return std::nullopt;
    }
    std::ifstream f(filename);
    nlohmann::json j;
    f >> j;
    return j.get<WifiCardSettings>();
  }
};


static nlohmann::json wificards_to_json(const std::vector<WiFiCard> &cards) {
  nlohmann::json j;
  for (auto &_card: cards) {
	nlohmann::json cardJson = _card;
	j.push_back(cardJson);
  }
  return j;
}

static constexpr auto WIFI_MANIFEST_FILENAME = "/tmp/wifi_manifest";

static void write_wificards_manifest(const std::vector<WiFiCard> &cards) {
  auto manifest = wificards_to_json(cards);
  std::ofstream _t(WIFI_MANIFEST_FILENAME);
  _t << manifest.dump(4);
  _t.close();
}


#endif
