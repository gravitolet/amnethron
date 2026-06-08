#include "include/database/entities/Profile.h"
#include "include/global/HTTPRequestHelper.hpp"

#include "include/configs/sub/GroupUpdater.hpp"
#include "include/configs/sub/clash.hpp"

#include <QInputDialog>
#include <QUrlQuery>
#include <QUrl>
#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include "include/configs/common/utils.h"
#include "include/database/GroupsRepo.h"
#include "include/database/ProfilesRepo.h"

namespace Subscription {

    GroupUpdater *groupUpdater = new GroupUpdater;

    int JsonEndIdx(const QString &str, int begin) {
        int sz = str.length();
        int counter = 1;
        for (int i=begin+1;i<sz;i++) {
            if (str[i] == '{') counter++;
            if (str[i] == '}') counter--;
            if (counter==0) return i;
        }
        return -1;
    }

    QList<QString> Disect(const QString &str) {
        QList<QString> res = QList<QString>();
        int idx=0;
        int sz = str.size();
        while(idx < sz) {
            if (str[idx] == '\n') {
                idx++;
                continue;
            }
            if (str[idx] == '{') {
                int endIdx = JsonEndIdx(str, idx);
                if (endIdx == -1) return res;
                res.append(str.mid(idx, endIdx-idx + 1));
                idx = endIdx+1;
                continue;
            }
            int nlineIdx = str.indexOf('\n', idx);
            if (nlineIdx == -1) nlineIdx = sz;
            res.append(str.mid(idx, nlineIdx-idx));
            idx = nlineIdx+1;
        }
        return res;
    }

    SingBoxSubType getSingBoxSubType(const QJsonDocument &doc) {
        if (doc.isObject()) {
            auto obj = doc.object();
            bool hasInbound = obj.contains("inbounds");
            bool hasOutbound = obj.contains("outbounds") || obj.contains("endpoints");
            // if (hasInbound && hasOutbound) return SingBoxSubType::fullConfig;
            if (hasOutbound) return SingBoxSubType::outboundInJson;
            if (obj.contains("type")) return SingBoxSubType::outboundObject;
            return SingBoxSubType::invalid;
        }
        if (doc.isArray() && !doc.array().empty()) {
            auto arr = doc.array();
            auto firstRaw = arr.first();
            if (firstRaw.isObject()) {
                auto obj = firstRaw.toObject();
                if (obj.contains("type")) return SingBoxSubType::outboundJsonArray;
            }
            return SingBoxSubType::invalid;
        }
        return SingBoxSubType::invalid;
    }

    // Xray uses "protocol" instead of sing-box's "type" field on outbounds, so
    // we can disambiguate by inspecting individual outbound objects rather than
    // the wrapper.
    XraySubType getXraySubType(const QJsonDocument &doc) {
        if (doc.isObject()) {
            auto obj = doc.object();
            if (obj.contains("outbounds")) {
                for (const auto &item : obj["outbounds"].toArray()) {
                    if (item.isObject() && item.toObject().contains("protocol")) {
                        return XraySubType::outboundInJson;
                    }
                }
            }
            if (obj.contains("protocol")) return XraySubType::outboundObject;
            return XraySubType::invalid;
        }
        if (doc.isArray() && !doc.array().empty()) {
            auto first = doc.array().first();
            if (first.isObject() && first.toObject().contains("protocol")) {
                return XraySubType::outboundJsonArray;
            }
        }
        return XraySubType::invalid;
    }

    // Convert a real Xray VLESS outbound (settings.vnext[0].address etc.) into
    // the simplified shape Throne's xrayVless::ParseFromJson expects. Returns
    // an empty object if the input doesn't have the expected structure.
    QJsonObject normalizeXrayVlessForParse(const QJsonObject &out) {
        if (out["protocol"].toString() != "vless") return {};
        auto settings = out["settings"].toObject();
        // Already in simplified form.
        if (settings.contains("address") && !settings.contains("vnext")) return out;
        auto vnext = settings["vnext"].toArray();
        if (vnext.isEmpty()) return {};
        auto first = vnext.first().toObject();
        if (first.isEmpty()) return {};
        auto users = first["users"].toArray();
        if (users.isEmpty()) return {};
        auto user = users.first().toObject();
        QJsonObject simpleSettings;
        simpleSettings["address"] = first["address"];
        simpleSettings["port"] = first["port"];
        simpleSettings["id"] = user["id"];
        simpleSettings["encryption"] = user.contains("encryption") ? user["encryption"] : QJsonValue("none");
        simpleSettings["flow"] = user["flow"];
        QJsonObject normalized = out;
        normalized["settings"] = simpleSettings;
        return normalized;
    }

    std::shared_ptr<Configs::Profile> makeProfileForXrayOutbound(const QJsonObject &out) {
        if (out.isEmpty()) return nullptr;
        auto protocol = out["protocol"].toString();
        // System protocols don't make sense as user profiles.
        if (protocol == "freedom" || protocol == "blackhole" || protocol == "dns" || protocol == "loopback") {
            return nullptr;
        }
        std::shared_ptr<Configs::Profile> ent;
        if (protocol == "vless") {
            if (auto normalized = normalizeXrayVlessForParse(out); !normalized.isEmpty()) {
                ent = Configs::ProfilesRepo::NewProfile("xrayvless");
                if (ent->XrayVLESS()->ParseFromJson(normalized)) return ent;
            }
        }
        ent = Configs::ProfilesRepo::NewProfile("custom");
        ent->Custom()->type = Configs::Custom::CustomXrayOutbound;
        ent->Custom()->config = QJsonObject2QString(out, false);
        if (auto tag = out["tag"].toString(); !tag.isEmpty()) ent->Custom()->name = tag;
        return ent;
    }

    QString jsonScalarToString(const QJsonValue &value) {
        if (value.isString()) return value.toString();
        if (value.isDouble()) {
            double d = value.toDouble();
            qint64 i = static_cast<qint64>(d);
            if (d == static_cast<double>(i)) return QString::number(i);
            return QString::number(d, 'g', 15);
        }
        if (value.isBool()) return value.toBool() ? "true" : "false";
        return {};
    }

    QJsonValue pickJsonValue(const QList<QJsonObject> &objects, const QStringList &keys) {
        for (const auto &object : objects) {
            if (object.isEmpty()) continue;
            for (const auto &key : keys) {
                auto value = object.value(key);
                if (!value.isUndefined() && !value.isNull()) return value;
            }
        }
        return {};
    }

    QString pickString(const QList<QJsonObject> &objects, const QStringList &keys) {
        return jsonScalarToString(pickJsonValue(objects, keys)).trimmed();
    }

    int pickInt(const QList<QJsonObject> &objects, const QStringList &keys, int defaultValue = 0) {
        auto value = pickJsonValue(objects, keys);
        if (value.isDouble()) return value.toInt(defaultValue);
        bool ok = false;
        auto result = jsonScalarToString(value).toInt(&ok);
        return ok ? result : defaultValue;
    }

    QStringList pickStringList(const QList<QJsonObject> &objects, const QStringList &keys) {
        auto value = pickJsonValue(objects, keys);
        if (value.isArray()) {
            QStringList result;
            for (const auto &item : value.toArray()) {
                auto text = jsonScalarToString(item).trimmed();
                if (!text.isEmpty()) result << text;
            }
            return result;
        }

        auto text = jsonScalarToString(value).trimmed();
        if (text.isEmpty()) return {};
        if (text.contains(",")) return SplitAndTrim(text, ",", false);
        return {text};
    }

    QJsonObject jsonObjectFromValue(const QJsonValue &value) {
        if (value.isObject()) return value.toObject();
        auto text = value.toString().trimmed();
        if (text.isEmpty()) return {};

        QJsonParseError error;
        auto doc = QJsonDocument::fromJson(text.toUtf8(), &error);
        if (error.error != QJsonParseError::NoError || !doc.isObject()) return {};
        return doc.object();
    }

    QByteArray decodeAmneziaVpnPayload(const QString &str) {
        auto payload = str.trimmed();
        if (payload.startsWith("vpn://", Qt::CaseInsensitive)) {
            payload = payload.mid(QStringLiteral("vpn://").size());
        }
        auto decoded = DecodeB64IfValid(payload, QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        if (decoded.isEmpty()) return {};

        auto uncompressed = qUncompress(decoded);
        return uncompressed.isEmpty() ? decoded : uncompressed;
    }

    QJsonObject protocolConfigFromAmneziaContainer(const QJsonObject &container) {
        auto awg = container.value("awg").toObject();
        if (!awg.isEmpty()) return awg;

        auto wireguard = container.value("wireguard").toObject();
        if (!wireguard.isEmpty()) return wireguard;

        for (auto it = container.constBegin(); it != container.constEnd(); ++it) {
            if (!it.value().isObject()) continue;
            auto object = it.value().toObject();
            if (object.contains("last_config") || object.contains("client_priv_key") || object.contains("config")) {
                return object;
            }
        }
        return {};
    }

    QString bracketIpv6EndpointHost(const QString &host) {
        if (host.contains(":") && !host.startsWith("[") && !host.endsWith("]")) {
            return "[" + host + "]";
        }
        return host;
    }

    QStringList defaultWireGuardAllowedIps(const QString &address) {
        bool hasIPv4 = false;
        bool hasIPv6 = false;
        for (auto item : address.split(",", Qt::SkipEmptyParts)) {
            item = item.trimmed();
            const auto slash = item.indexOf("/");
            if (slash >= 0) item = item.left(slash);
            if (IsIpAddressV4(item)) hasIPv4 = true;
            if (IsIpAddressV6(item)) hasIPv6 = true;
        }

        QStringList allowedIps;
        if (hasIPv4) allowedIps << "0.0.0.0/0";
        if (hasIPv6) allowedIps << "::/0";
        if (allowedIps.isEmpty()) allowedIps << "0.0.0.0/0" << "::/0";
        return allowedIps;
    }

    QString makeWireGuardConfigFromAmneziaJson(
        const QJsonObject &root,
        const QJsonObject &protocolConfig,
        const QJsonObject &clientConfig
    ) {
        const QList<QJsonObject> clientFirst{clientConfig, protocolConfig, root};
        const QList<QJsonObject> protocolFirst{protocolConfig, clientConfig, root};

        QString privateKey = pickString(clientFirst, {"client_priv_key", "private_key", "PrivateKey"});
        QString address = pickString(clientFirst, {"client_ip", "address", "Address"});
        QString serverPublicKey = pickString(clientFirst, {"server_pub_key", "public_key", "PublicKey"});
        QString presharedKey = pickString(clientFirst, {"psk_key", "pre_shared_key", "PresharedKey", "PreSharedKey"});
        QString mtu = pickString(clientFirst, {"mtu", "MTU"});
        QString keepAlive = pickString(clientFirst, {"persistent_keep_alive", "persistent_keepalive", "PersistentKeepalive"});

        QString host = pickString(clientFirst, {"hostName", "host_name", "server", "endpoint_host"});
        QString port = pickString(clientFirst, {"port", "server_port"});
        auto endpoint = pickString(clientFirst, {"Endpoint", "endpoint"});
        if ((!endpoint.isEmpty()) && (host.isEmpty() || port.isEmpty())) {
            auto endpointUrl = QUrl::fromUserInput(endpoint);
            if (!endpointUrl.host().isEmpty()) host = endpointUrl.host();
            if (endpointUrl.port() > 0) port = QString::number(endpointUrl.port());
        }
        if (host.isEmpty() || port.isEmpty()) return {};

        QStringList lines;
        auto appendLine = [&lines](const QString &key, const QString &value) {
            if (!value.isEmpty()) lines << QString("%1 = %2").arg(key, value);
        };

        lines << "[Interface]";
        appendLine("PrivateKey", privateKey);
        appendLine("Address", address);

        QStringList dns;
        auto dns1 = pickString(clientFirst, {"dns1"});
        auto dns2 = pickString(clientFirst, {"dns2"});
        if (!dns1.isEmpty()) dns << dns1;
        if (!dns2.isEmpty()) dns << dns2;
        if (!dns.isEmpty()) appendLine("DNS", dns.join(", "));

        appendLine("MTU", mtu);
        appendLine("Jc", pickString(protocolFirst, {"Jc", "jc"}));
        appendLine("Jmin", pickString(protocolFirst, {"Jmin", "jmin"}));
        appendLine("Jmax", pickString(protocolFirst, {"Jmax", "jmax"}));
        appendLine("S1", pickString(protocolFirst, {"S1", "s1"}));
        appendLine("S2", pickString(protocolFirst, {"S2", "s2"}));
        appendLine("S3", pickString(protocolFirst, {"S3", "s3"}));
        appendLine("S4", pickString(protocolFirst, {"S4", "s4"}));
        appendLine("H1", pickString(protocolFirst, {"H1", "h1"}));
        appendLine("H2", pickString(protocolFirst, {"H2", "h2"}));
        appendLine("H3", pickString(protocolFirst, {"H3", "h3"}));
        appendLine("H4", pickString(protocolFirst, {"H4", "h4"}));
        appendLine("I1", pickString(protocolFirst, {"I1", "i1"}));
        appendLine("I2", pickString(protocolFirst, {"I2", "i2"}));
        appendLine("I3", pickString(protocolFirst, {"I3", "i3"}));
        appendLine("I4", pickString(protocolFirst, {"I4", "i4"}));
        appendLine("I5", pickString(protocolFirst, {"I5", "i5"}));

        lines << "" << "[Peer]";
        appendLine("PublicKey", serverPublicKey);
        appendLine("PresharedKey", presharedKey);
        appendLine("Endpoint", QString("%1:%2").arg(bracketIpv6EndpointHost(host), port));
        appendLine("PersistentKeepalive", keepAlive);

        auto allowedIps = pickStringList(clientFirst, {"allowed_ips", "AllowedIPs"});
        if (allowedIps.isEmpty()) allowedIps = defaultWireGuardAllowedIps(address);
        appendLine("AllowedIPs", allowedIps.join(", "));

        return lines.join("\n");
    }

    void applyAmneziaOptions(
        Configs::wireguard *wireguard,
        const QJsonObject &root,
        const QJsonObject &protocolConfig,
        const QJsonObject &clientConfig
    ) {
        const QList<QJsonObject> objects{clientConfig, protocolConfig, root};
        wireguard->enable_amnezia = true;

        auto setInt = [&objects](int &target, const QStringList &keys) {
            int value = pickInt(objects, keys, target);
            if (value > 0) target = value;
        };
        auto setString = [&objects](QString &target, const QStringList &keys) {
            auto value = pickString(objects, keys);
            if (!value.isEmpty()) target = value;
        };

        setInt(wireguard->jc, {"Jc", "jc"});
        setInt(wireguard->jmin, {"Jmin", "jmin"});
        setInt(wireguard->jmax, {"Jmax", "jmax"});
        setInt(wireguard->s1, {"S1", "s1"});
        setInt(wireguard->s2, {"S2", "s2"});
        setInt(wireguard->s3, {"S3", "s3"});
        setInt(wireguard->s4, {"S4", "s4"});
        setString(wireguard->h1, {"H1", "h1"});
        setString(wireguard->h2, {"H2", "h2"});
        setString(wireguard->h3, {"H3", "h3"});
        setString(wireguard->h4, {"H4", "h4"});
        setString(wireguard->i1, {"I1", "i1"});
        setString(wireguard->i2, {"I2", "i2"});
        setString(wireguard->i3, {"I3", "i3"});
        setString(wireguard->i4, {"I4", "i4"});
        setString(wireguard->i5, {"I5", "i5"});
    }

    std::shared_ptr<Configs::Profile> makeProfileFromAmneziaWireGuard(
        const QJsonObject &root,
        const QJsonObject &protocolConfig,
        const QJsonObject &clientConfig
    ) {
        auto ent = Configs::ProfilesRepo::NewProfile("wireguard");
        auto wireguard = ent->Wireguard();
        if (!wireguard) return nullptr;

        bool ok = false;
        auto nativeConfig = clientConfig.value("config").toString();
        if (nativeConfig.contains("[Interface]") && nativeConfig.contains("[Peer]")) {
            ok = wireguard->ParseFromLink(nativeConfig);
        }

        if (!ok) {
            auto generatedConfig = makeWireGuardConfigFromAmneziaJson(root, protocolConfig, clientConfig);
            if (generatedConfig.isEmpty()) return nullptr;
            ok = wireguard->ParseFromLink(generatedConfig);
        }
        if (!ok) return nullptr;

        applyAmneziaOptions(wireguard, root, protocolConfig, clientConfig);

        const QList<QJsonObject> nameObjects{root, clientConfig, protocolConfig};
        auto name = pickString(nameObjects, {"name", "description", "displayName", "tag"});
        if (!name.isEmpty()) wireguard->name = name;

        return ent;
    }

    std::shared_ptr<Configs::Profile> makeProfileFromAmneziaConfig(const QJsonObject &root) {
        auto containers = root.value("containers").toArray();
        for (const auto &item : containers) {
            if (!item.isObject()) continue;
            auto container = item.toObject();
            auto containerName = container.value("container").toString().toLower();
            auto protocolConfig = protocolConfigFromAmneziaContainer(container);
            if (protocolConfig.isEmpty()) continue;
            if (!containerName.contains("awg") && !container.contains("awg")) continue;

            auto clientConfig = jsonObjectFromValue(protocolConfig.value("last_config"));
            if (clientConfig.isEmpty()) clientConfig = protocolConfig;

            if (auto ent = makeProfileFromAmneziaWireGuard(root, protocolConfig, clientConfig); ent != nullptr) {
                return ent;
            }
        }

        auto protocolConfig = protocolConfigFromAmneziaContainer(root);
        auto clientConfig = jsonObjectFromValue(protocolConfig.value("last_config"));
        if (clientConfig.isEmpty()) clientConfig = protocolConfig;
        if (!protocolConfig.isEmpty()) {
            return makeProfileFromAmneziaWireGuard(root, protocolConfig, clientConfig);
        }

        return nullptr;
    }

    void RawUpdater::update(const QString &str, bool needParse, bool isBase64Decoded) {
        const QString trimmedInput = str.trimmed();

        if (trimmedInput.startsWith("vpn://", Qt::CaseInsensitive)) {
            MW_show_log(">>>>>>>> " + QObject::tr("Detected Amnezia VPN key..."));
            updateAmneziaVpnLink(trimmedInput);
            return;
        }

        // Base64 encoded subscription
        if (!isBase64Decoded) {
            if (auto str2 = DecodeB64IfValid(trimmedInput); !str2.isEmpty()) {
                update(str2, true, true);
                return;
            }
        }

        std::shared_ptr<Configs::Profile> ent;

        // Json
        QJsonParseError error;
        auto doc = QJsonDocument::fromJson(str.toUtf8(), &error);
        if (error.error == QJsonParseError::NoError) {
            if (doc.isObject()) {
                if (auto e = makeProfileFromAmneziaConfig(doc.object()); e != nullptr) {
                    updated_order += e;
                    return;
                }
            }

            // Xray (checked first since its outbounds are tagged with
            // "protocol", which lets us cleanly disambiguate from sing-box
            // configs that share the "outbounds" wrapper).
            auto xrayType = getXraySubType(doc);
            if (xrayType == XraySubType::outboundObject) {
                if (auto e = makeProfileForXrayOutbound(doc.object()); e != nullptr) {
                    updated_order += e;
                }
                return;
            }
            if (xrayType == XraySubType::outboundInJson || xrayType == XraySubType::outboundJsonArray) {
                updateXray(doc, xrayType);
                return;
            }

            // SingBox
            auto subType = getSingBoxSubType(doc);
            if (subType == SingBoxSubType::fullConfig) {
                ent = Configs::ProfilesRepo::NewProfile("custom");
                ent->Custom()->type = Configs::Custom::CustomFullConfig;
                ent->Custom()->config = str;
                updated_order += ent;
            } else if (subType == SingBoxSubType::outboundObject) {
                ent = Configs::ProfilesRepo::NewProfile("custom");
                ent->Custom()->type = Configs::Custom::CustomOutbound;
                ent->Custom()->config = str;
                updated_order += ent;
            } else if (subType == SingBoxSubType::outboundInJson || subType == SingBoxSubType::outboundJsonArray) {
                updateSingBox(doc, subType);
                return;
            }

            // SIP008
            if (str.contains("version") && str.contains("servers"))
            {
                updateSIP008(str);
                return;
            }

            return;
        }

        // Clash
        if (str.contains("proxies:")) {
            updateClash(str);
            return;
        }

        // Wireguard Config
        if (str.contains("[Interface]") && str.contains("[Peer]"))
        {
            updateWireguardFileConfig(str);
            return;
        }

        // Multi line
        if (str.count("\n") > 0 && needParse) {
            auto list = Disect(str);
            for (const auto &str2: list) {
                update(str2.trimmed(), false);
            }
            return;
        }

        // is comment or too short
        if (str.startsWith("//") || str.startsWith("#") || str.length() < 2) {
            return;
        }

        // Json base64 link format
        if (str.startsWith("json://")) {
            auto link = QUrl(str);
            if (!link.isValid()) return;
            auto dataBytes = DecodeB64IfValid(link.fragment().toUtf8(), QByteArray::Base64UrlEncoding);
            if (dataBytes.isEmpty()) return;
            auto data = QJsonDocument::fromJson(dataBytes).object();
            if (data.isEmpty()) return;
            if (data.contains("protocol")) {
                ent = Configs::ProfilesRepo::NewProfile("xray" + data["protocol"].toString());
            } else {
                ent = data["type"].toString() == "hysteria2" ? Configs::ProfilesRepo::NewProfile("hysteria") : Configs::ProfilesRepo::NewProfile(data["type"].toString());
            }
            if (ent->outbound->invalid) return;
            ent->outbound->ParseFromJson(data);
        }

        // Json
        if (str.startsWith('{')) {
            ent = Configs::ProfilesRepo::NewProfile("custom");
            auto custom = ent->Custom();
            auto obj = QString2QJsonObject(str);
            if (obj.contains("outbounds")) {
                custom->type = Configs::Custom::CustomFullConfig;
                custom->config = str;
            } else if (obj.contains("server")) {
                custom->type = Configs::Custom::CustomOutbound;
                custom->config = str;
            } else {
                return;
            }
        }

        // SOCKS
        if (str.startsWith("socks5://") || str.startsWith("socks4://") ||
            str.startsWith("socks4a://") || str.startsWith("socks://")) {
            ent = Configs::ProfilesRepo::NewProfile("socks");
            auto ok = ent->Socks()->ParseFromLink(str);
            if (!ok) return;
        }

        // HTTP
        if (str.startsWith("http://") || str.startsWith("https://")) {
            ent = Configs::ProfilesRepo::NewProfile("http");
            auto ok = ent->Http()->ParseFromLink(str);
            if (!ok) return;
        }

        // ShadowSocks
        if (str.startsWith("ss://")) {
            ent = Configs::ProfilesRepo::NewProfile("shadowsocks");
            auto ok = ent->ShadowSocks()->ParseFromLink(str);
            if (!ok) return;
        }

        // VMess
        if (str.startsWith("vmess://")) {
            ent = Configs::ProfilesRepo::NewProfile("vmess");
            auto ok = ent->VMess()->ParseFromLink(str);
            if (!ok) return;
        }

        // VLESS
        if (str.startsWith("vless://")) {
            if (Configs::useXrayVless(str)) {
                ent = Configs::ProfilesRepo::NewProfile("xrayvless");
                auto ok = ent->XrayVLESS()->ParseFromLink(str);
                if (!ok) return;
            } else {
                ent = Configs::ProfilesRepo::NewProfile("vless");
                auto ok = ent->VLESS()->ParseFromLink(str);
                if (!ok) return;
            }
        }

        // Trojan
        if (str.startsWith("trojan://")) {
            ent = Configs::ProfilesRepo::NewProfile("trojan");
            auto ok = ent->Trojan()->ParseFromLink(str);
            if (!ok) return;
        }

        // AnyTLS
        if (str.startsWith("anytls://")) {
            ent = Configs::ProfilesRepo::NewProfile("anytls");
            auto ok = ent->AnyTLS()->ParseFromLink(str);
            if (!ok) return;
        }

        // Hysteria
        if (str.startsWith("hysteria://") || str.startsWith("hysteria2://") || str.startsWith("hy2://")) {
            ent = Configs::ProfilesRepo::NewProfile("hysteria");
            auto ok = ent->Hysteria()->ParseFromLink(str);
            if (!ok) return;
        }

        // TUIC
        if (str.startsWith("tuic://")) {
            ent = Configs::ProfilesRepo::NewProfile("tuic");
            auto ok = ent->TUIC()->ParseFromLink(str);
            if (!ok) return;
        }

        // Juicity
        if (str.startsWith("juicity://")) {
            ent = Configs::ProfilesRepo::NewProfile("juicity");
            auto ok = ent->Juicity()->ParseFromLink(str);
            if (!ok) return;
        }

        // TrustTunnel
        if (str.startsWith("tt://")) {
            ent = Configs::ProfilesRepo::NewProfile("trusttunnel");
            auto ok = ent->TrustTunnel()->ParseFromLink(str);
            if (!ok) return;
        }

        // ShadowTLS
        if (str.startsWith("shadowtls://")) {
            ent = Configs::ProfilesRepo::NewProfile("shadowtls");
            auto ok = ent->ShadowTLS()->ParseFromLink(str);
            if (!ok) return;
        }

        // Wireguard
        if (str.startsWith("wg://")) {
            ent = Configs::ProfilesRepo::NewProfile("wireguard");
            auto ok = ent->Wireguard()->ParseFromLink(str);
            if (!ok) return;
        }

        // SSH
        if (str.startsWith("ssh://")) {
            ent = Configs::ProfilesRepo::NewProfile("ssh");
            auto ok = ent->SSH()->ParseFromLink(str);
            if (!ok) return;
        }

        // Naive
        if (str.startsWith("naive+https://") || str.startsWith("naive+quic://")) {
            ent = Configs::ProfilesRepo::NewProfile("naive");
            auto ok = ent->Naive()->ParseFromLink(str);
            if (!ok) return;
        }

        if (ent == nullptr) return;

        // End
        updated_order += ent;
    }

    void RawUpdater::updateSingBox(const QJsonDocument &doc, SingBoxSubType type)
    {
        QJsonArray outbounds, endpoints;
        if (type == SingBoxSubType::outboundInJson) {
            auto json = doc.object();
            outbounds = json["outbounds"].toArray();
            endpoints = json["endpoints"].toArray();
        } else if (type == SingBoxSubType::outboundJsonArray) {
            outbounds = doc.array();
        } else {
            return;
        }
        QJsonArray items;
        for (const auto& outbound : outbounds)
        {
            if (!outbound.isObject()) continue;
            items.append(outbound.toObject());
        }
        for (const auto& endpoint : endpoints)
        {
            if (!endpoint.isObject()) continue;
            items.append(endpoint.toObject());
        }

        for (const auto& o : items)
        {
            auto out = o.toObject();
            if (out.isEmpty())
            {
                MW_show_log("invalid outbound of type: " + o.type());
                continue;
            }

            std::shared_ptr<Configs::Profile> ent;

            // SOCKS
            if (out["type"] == "socks") {
                ent = Configs::ProfilesRepo::NewProfile("socks");
                auto ok = ent->Socks()->ParseFromJson(out);
                if (!ok) continue;
            }

            // HTTP
            if (out["type"] == "http") {
                ent = Configs::ProfilesRepo::NewProfile("http");
                auto ok = ent->Http()->ParseFromJson(out);
                if (!ok) continue;
            }

            // ShadowSocks
            if (out["type"] == "shadowsocks") {
                ent = Configs::ProfilesRepo::NewProfile("shadowsocks");
                auto ok = ent->ShadowSocks()->ParseFromJson(out);
                if (!ok) continue;
            }

            // VMess
            if (out["type"] == "vmess") {
                ent = Configs::ProfilesRepo::NewProfile("vmess");
                auto ok = ent->VMess()->ParseFromJson(out);
                if (!ok) continue;
            }

            // VLESS
            if (out["type"] == "vless") {
                ent = Configs::ProfilesRepo::NewProfile("vless");
                auto ok = ent->VLESS()->ParseFromJson(out);
                if (!ok) continue;
            }

            // Trojan
            if (out["type"] == "trojan") {
                ent = Configs::ProfilesRepo::NewProfile("trojan");
                auto ok = ent->Trojan()->ParseFromJson(out);
                if (!ok) continue;
            }

            // AnyTLS
            if (out["type"] == "anytls") {
                ent = Configs::ProfilesRepo::NewProfile("anytls");
                auto ok = ent->AnyTLS()->ParseFromJson(out);
                if (!ok) continue;
            }

            // Hysteria
            if (out["type"] == "hysteria" || out["type"] == "hysteria2") {
                ent = Configs::ProfilesRepo::NewProfile("hysteria");
                auto ok = ent->Hysteria()->ParseFromJson(out);
                if (!ok) continue;
            }

            // TUIC
            if (out["type"] == "tuic") {
                ent = Configs::ProfilesRepo::NewProfile("tuic");
                auto ok = ent->TUIC()->ParseFromJson(out);
                if (!ok) continue;
            }

            // Juicity
            if (out["type"] == "juicity") {
                ent = Configs::ProfilesRepo::NewProfile("juicity");
                auto ok = ent->Juicity()->ParseFromJson(out);
                if (!ok) continue;
            }

            // TrustTunnel
            if (out["type"] == "trusttunnel") {
                ent = Configs::ProfilesRepo::NewProfile("trusttunnel");
                auto ok = ent->TrustTunnel()->ParseFromJson(out);
                if (!ok) continue;
            }

            // ShadowTLS
            if (out["type"] == "shadowtls") {
                ent = Configs::ProfilesRepo::NewProfile("shadowtls");
                auto ok = ent->ShadowTLS()->ParseFromJson(out);
                if (!ok) continue;
            }

            // Wireguard
            if (out["type"] == "wireguard") {
                ent = Configs::ProfilesRepo::NewProfile("wireguard");
                auto ok = ent->Wireguard()->ParseFromJson(out);
                if (!ok) continue;
            }

            // SSH
            if (out["type"] == "ssh") {
                ent = Configs::ProfilesRepo::NewProfile("ssh");
                auto ok = ent->SSH()->ParseFromJson(out);
                if (!ok) continue;
            }

            // Naive
            if (out["type"] == "naive") {
                ent = Configs::ProfilesRepo::NewProfile("naive");
                auto ok = ent->Naive()->ParseFromJson(out);
                if (!ok) continue;
            }

            if (ent == nullptr) continue;

            updated_order += ent;
        }
    }

    void RawUpdater::updateXray(const QJsonDocument &doc, XraySubType type)
    {
        QJsonArray outbounds;
        if (type == XraySubType::outboundInJson) {
            outbounds = doc.object()["outbounds"].toArray();
        } else if (type == XraySubType::outboundJsonArray) {
            outbounds = doc.array();
        } else {
            return;
        }
        for (const auto &o : outbounds) {
            if (!o.isObject()) continue;
            if (auto e = makeProfileForXrayOutbound(o.toObject()); e != nullptr) {
                updated_order += e;
            }
        }
    }

    void RawUpdater::updateClash(const QString& str)
    {
        try {
            fkyaml::node node = fkyaml::node::deserialize(str.toStdString());
            clash::Clash clash_config = node.get_value<clash::Clash>();
    
            for (const auto& out : clash_config.proxies)
            {
                std::shared_ptr<Configs::Profile> ent;
    
                // SOCKS
                if (out.type == "socks5") {
                    ent = Configs::ProfilesRepo::NewProfile("socks");
                    auto ok = ent->Socks()->ParseFromClash(out);
                    if (!ok) continue;
                }
    
                // HTTP
                if (out.type == "http") {
                    ent = Configs::ProfilesRepo::NewProfile("http");
                    auto ok = ent->Http()->ParseFromClash(out);
                    if (!ok) continue;
                }
    
                // ShadowSocks
                if (out.type == "ss") {
                    ent = Configs::ProfilesRepo::NewProfile("shadowsocks");
                    auto ok = ent->ShadowSocks()->ParseFromClash(out);
                    if (!ok) continue;
                }
    
                // VMess
                if (out.type == "vmess") {
                    ent = Configs::ProfilesRepo::NewProfile("vmess");
                    auto ok = ent->VMess()->ParseFromClash(out);
                    if (!ok) continue;
                }
    
                // VLESS
                if (out.type == "vless") {
                    if (out.network == "xhttp" || (!out.encryption.empty() && out.encryption != "none")) {
                        ent = Configs::ProfilesRepo::NewProfile("xrayvless");
                        auto ok = ent->XrayVLESS()->ParseFromClash(out);
                        if (!ok) continue;
                    } else {
                        ent = Configs::ProfilesRepo::NewProfile("vless");
                        auto ok = ent->VLESS()->ParseFromClash(out);
                        if (!ok) continue;
                    }
                }
    
                // Trojan
                if (out.type == "trojan") {
                    ent = Configs::ProfilesRepo::NewProfile("trojan");
                    auto ok = ent->Trojan()->ParseFromClash(out);
                    if (!ok) continue;
                }
    
                // AnyTLS
                if (out.type == "anytls") {
                    ent = Configs::ProfilesRepo::NewProfile("anytls");
                    auto ok = ent->AnyTLS()->ParseFromClash(out);
                    if (!ok) continue;
                }
    
                // Hysteria
                if (out.type == "hysteria" || out.type == "hysteria2") {
                    ent = Configs::ProfilesRepo::NewProfile("hysteria");
                    auto ok = ent->Hysteria()->ParseFromClash(out);
                    if (!ok) continue;
                }
    
                // TUIC
                if (out.type == "tuic") {
                    ent = Configs::ProfilesRepo::NewProfile("tuic");
                    auto ok = ent->TUIC()->ParseFromClash(out);
                    if (!ok) continue;
                }
    
                // SSH
                if (out.type == "ssh") {
                    ent = Configs::ProfilesRepo::NewProfile("ssh");
                    auto ok = ent->SSH()->ParseFromClash(out);
                    if (!ok) continue;
                }
    
                if (ent == nullptr) continue;
    
                updated_order += ent;
            }
        } catch (const fkyaml::exception &ex) {
            runOnUiThread([=] {
                MessageBoxWarning("YAML Exception", ex.what());
            });
        }
    }

    void RawUpdater::updateAmneziaVpnLink(const QString& str)
    {
        auto payload = decodeAmneziaVpnPayload(str);
        if (payload.isEmpty()) {
            MW_show_log("<<<<<<<< " + QObject::tr("Amnezia VPN key decoding failed."));
            return;
        }

        QJsonParseError error;
        auto doc = QJsonDocument::fromJson(payload, &error);
        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            if (auto ent = makeProfileFromAmneziaConfig(doc.object()); ent != nullptr) {
                updated_order += ent;
                MW_show_log("<<<<<<<< " + QObject::tr("Imported Amnezia VPN key as Amnezia-WG profile."));
            } else {
                MW_show_log("<<<<<<<< " + QObject::tr("Amnezia VPN key decoded, but no supported Amnezia-WG container was found."));
            }
            return;
        }

        auto decodedText = QString::fromUtf8(payload);
        if (decodedText.trimmed().startsWith("[Interface]") && decodedText.contains("[Peer]")) {
            auto ent = Configs::ProfilesRepo::NewProfile("wireguard");
            auto ok = ent->Wireguard()->ParseFromLink(decodedText);
            if (!ok) {
                MW_show_log("<<<<<<<< " + QObject::tr("Amnezia VPN key decoded, but WireGuard config parsing failed."));
                return;
            }
            updated_order += ent;
            MW_show_log("<<<<<<<< " + QObject::tr("Imported Amnezia VPN key as WireGuard config."));
            return;
        }

        MW_show_log("<<<<<<<< " + QObject::tr("Amnezia VPN key is not a supported JSON or WireGuard config."));
    }

    void RawUpdater::updateWireguardFileConfig(const QString& str)
    {
        auto ent = Configs::ProfilesRepo::NewProfile("wireguard");
        auto ok = ent->Wireguard()->ParseFromLink(str);
        if (!ok) return;
        updated_order += ent;
    }

    void RawUpdater::updateSIP008(const QString& str)
    {
        auto json = QString2QJsonObject(str);

        for (const auto& o : json["servers"].toArray())
        {
            auto out = o.toObject();
            if (out.isEmpty())
            {
                MW_show_log("invalid server object");
                continue;
            }

            auto ent = Configs::ProfilesRepo::NewProfile("shadowsocks");
            auto ok = ent->ShadowSocks()->ParseFromSIP008(out);
            if (!ok) continue;
            updated_order += ent;
        }
    }

    // 在新的 thread 运行
    void GroupUpdater::AsyncUpdate(const QString &str, int _sub_gid, const std::function<void()> &finish) {
        auto content = str.trimmed();
        bool asURL = false;
        bool createNewGroup = false;

        if (_sub_gid < 0 && (content.startsWith("http://") || content.startsWith("https://"))) {
            auto items = QStringList{
                QObject::tr("Add profiles to this group"),
                QObject::tr("Create new subscription group"),
                QObject::tr("Import HTTP proxy profile"),
            };
            bool ok;
            auto a = QInputDialog::getItem(nullptr,
                                           QObject::tr("url detected"),
                                           QObject::tr("%1\nHow to update?").arg(content),
                                           items, 0, false, &ok);
            if (!ok) return;
            switch (items.indexOf(a)) {
                case 1: createNewGroup = true;
                case 0: asURL = true; break;
            }
        }

        runOnNewThread([=,this] {
            auto gid = _sub_gid;
            if (createNewGroup) {
                auto group = Configs::GroupsRepo::NewGroup();
                group->name = QUrl(str).host();
                group->url = str;
                Configs::dataManager->groupsRepo->AddGroup(group);
                gid = group->id;
                MW_dialog_message(MwMessage::SubscriptionNewGroup, {});
            }
            Update(str, gid, asURL);
            emit asyncUpdateCallback(gid);
            if (finish != nullptr) finish();
        });
    }

    void GroupUpdater::Update(const QString &_str, int _sub_gid, bool _not_sub_as_url) {
        // 创建 rawUpdater
        Configs::dataManager->settingsRepo->imported_count = 0;
        auto rawUpdater = std::make_unique<RawUpdater>();
        rawUpdater->gid_add_to = _sub_gid;

        // 准备
        QString sub_user_info;
        bool asURL = _sub_gid >= 0 || _not_sub_as_url; // 把 _str 当作 url 处理（下载内容）
        auto content = _str.trimmed();
        auto group = Configs::dataManager->groupsRepo->GetGroup(_sub_gid);
        if (group != nullptr && group->archive) return;

        // 网络请求
        if (asURL) {
            auto groupName = group == nullptr ? content : group->name;
            MW_show_log(">>>>>>>> " + QObject::tr("Requesting subscription: %1").arg(groupName));

            auto resp = NetworkRequestHelper::HttpGet(content, Configs::dataManager->settingsRepo->sub_send_hwid);
            if (!resp.error.isEmpty()) {
                MW_show_log("<<<<<<<< " + QObject::tr("Requesting subscription %1 error: %2").arg(groupName, resp.error + "\n" + resp.data));
                return;
            }

            content = resp.data;
            sub_user_info = NetworkRequestHelper::GetHeader(resp.header, "Subscription-UserInfo");

            MW_show_log("<<<<<<<< " + QObject::tr("Subscription request fininshed: %1").arg(groupName));
        }

        QList<std::shared_ptr<Configs::Profile>> in;

        if (group != nullptr) {
            group->sub_last_update = QDateTime::currentMSecsSinceEpoch() / 1000;
            group->info = sub_user_info;
            Configs::dataManager->groupsRepo->Save(group);
            //
            if (Configs::dataManager->settingsRepo->sub_clear) {
                MW_show_log(QObject::tr("Clearing servers..."));
                if (!Configs::dataManager->profilesRepo->BatchDeleteProfiles(group->profiles, Configs::dataManager->settingsRepo->allow_stopping_active_profile)) {
                    runOnUiThread([=] {
                        MessageBoxWarning("Internal Error", "DB Error when deleting profiles, Please try again.");
                    });
                    return;
                }
            } else {
                in = Configs::dataManager->profilesRepo->GetProfileBatch(group->Profiles());
            }
        }

        MW_show_log(">>>>>>>> " + QObject::tr("Processing subscription data..."));
        rawUpdater->update(content);
        content.clear();
        Configs::dataManager->profilesRepo->AddProfileBatch(rawUpdater->updated_order, rawUpdater->gid_add_to);
        MW_show_log(">>>>>>>> " + QObject::tr("Process complete, applying..."));

        if (group != nullptr) {
            QList<std::shared_ptr<Configs::Profile>> out_all;
            out_all = Configs::dataManager->profilesRepo->GetProfileBatch(group->Profiles());;

            QString change_text;

            if (Configs::dataManager->settingsRepo->sub_clear) {
                // all is new profile
                if (out_all.size() >= 1000) {
                    change_text += "[+] " + Int2String(out_all.size()) + " profiles\n";
                } else {
                    for (const auto &ent: out_all) {
                        change_text += "[+] " + ent->outbound->DisplayTypeAndName() + "\n";
                    }
                }
            } else {
                QList<std::shared_ptr<Configs::Profile>> update_keep;
                QList<std::shared_ptr<Configs::Profile>> update_del;
                QList<std::shared_ptr<Configs::Profile>> only_out;
                QList<std::shared_ptr<Configs::Profile>> only_in;
                QList<std::shared_ptr<Configs::Profile>> out;
                // find and delete not updated profile by ProfileFilter
                Configs::ProfileFilter::OnlyInSrc_ByPointer(out_all, in, out);
                Configs::ProfileFilter::OnlyInSrc(in, out, only_in, false);
                Configs::ProfileFilter::OnlyInSrc(out, in, only_out, false);
                Configs::ProfileFilter::Common(in, out, update_keep, update_del, false);
                QString notice_added;
                QString notice_deleted;
                if (only_out.size() < 1000)
                {
                    for (const auto &ent: only_out) {
                        notice_added += "[+] " + ent->outbound->DisplayTypeAndName() + "\n";
                    }
                } else
                {
                    notice_added += QString("[+] ") + "added " + Int2String(only_out.size()) + "\n";
                }
                if (only_in.size() < 1000)
                {
                    for (const auto &ent: only_in) {
                        notice_deleted += "[-] " + ent->outbound->DisplayTypeAndName() + "\n";
                    }
                } else
                {
                    notice_deleted += QString("[-] ") + "deleted " + Int2String(only_in.size()) + "\n";
                }


                // sort according to order in remote
                group->profiles.clear();
                for (const auto &ent: rawUpdater->updated_order) {
                    auto deleted_index = update_del.indexOf(ent);
                    if (deleted_index >= 0) {
                        if (deleted_index >= update_keep.count()) continue; // should not happen
                        const auto& ent2 = update_keep[deleted_index];
                        group->profiles.append(ent2->id);
                    } else {
                        group->profiles.append(ent->id);
                    }
                }
                Configs::dataManager->groupsRepo->Save(group);

                // cleanup
                QList<int> del_ids;
                for (const auto &ent: out_all) {
                    if (!group->HasProfile(ent->id)) {
                        del_ids.append(ent->id);
                    }
                }
                if (!Configs::dataManager->profilesRepo->BatchDeleteProfiles(del_ids, Configs::dataManager->settingsRepo->allow_stopping_active_profile)) {
                    runOnUiThread([=] {
                       MessageBoxWarning("Internal error", "DB Error when deleting profiles, data may be corrupted");
                    });
                }

                change_text = "\n" + QObject::tr("Added %1 profiles:\n%2\nDeleted %3 Profiles:\n%4")
                                         .arg(only_out.length())
                                         .arg(notice_added)
                                         .arg(only_in.length())
                                         .arg(notice_deleted);
                if (only_out.length() + only_in.length() == 0) change_text = QObject::tr("Nothing");
            }

            MW_show_log("<<<<<<<< " + QObject::tr("Change of %1:").arg(group->name) + "\n" + change_text);
            MW_dialog_message(MwMessage::SubscriptionFinished, {MwArg::Quiet});
        } else {
            Configs::dataManager->settingsRepo->imported_count = rawUpdater->updated_order.count();
            MW_dialog_message(MwMessage::SubscriptionFinished, {});
        }
    }
} // namespace Subscription

bool UI_update_all_groups_Updating = false;

#define should_skip_group(g) (g == nullptr || g->url.isEmpty() || g->archive || (onlyAllowed && g->skip_auto_update))

void serialUpdateSubscription(const QList<int> &groupsTabOrder, int _order, bool onlyAllowed) {
    if (_order >= groupsTabOrder.size()) {
        UI_update_all_groups_Updating = false;
        return;
    }

    // calculate this group
    auto group = Configs::dataManager->groupsRepo->GetGroup(groupsTabOrder[_order]);
    if (group == nullptr || should_skip_group(group)) {
        serialUpdateSubscription(groupsTabOrder, _order + 1, onlyAllowed);
        return;
    }

    int nextOrder = _order + 1;
    while (nextOrder < groupsTabOrder.size()) {
        auto nextGid = groupsTabOrder[nextOrder];
        auto nextGroup = Configs::dataManager->groupsRepo->GetGroup(nextGid);
        if (!should_skip_group(nextGroup)) {
            break;
        }
        nextOrder += 1;
    }

    // Async update current group
    UI_update_all_groups_Updating = true;
    Subscription::groupUpdater->AsyncUpdate(group->url, group->id, [=] {
        serialUpdateSubscription(groupsTabOrder, nextOrder, onlyAllowed);
    });
}

void UI_update_all_groups(bool onlyAllowed) {
    if (UI_update_all_groups_Updating) {
        MW_show_log("The last subscription update has not exited.");
        return;
    }

    auto groupsTabOrder = Configs::dataManager->groupsRepo->GetGroupsTabOrder();
    serialUpdateSubscription(groupsTabOrder, 0, onlyAllowed);
}
