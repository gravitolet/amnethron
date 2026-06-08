#pragma once
#include "include/configs/common/Outbound.h"

namespace Configs
{
    class Peer : public baseConfig
    {
        public:
        QString address;
        int port = 0;
        QString public_key;
        QString pre_shared_key;
        QList<int> reserved;
        int persistent_keepalive = 0;
        QStringList allowed_ips;

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;
    };

    class wireguard : public outbound
    {
        public:
        QString private_key;
        std::shared_ptr<Peer> peer = std::make_shared<Peer>();
        QStringList address;
        int mtu = 1420;
        bool system = false;
        int worker_count = 0;
        QString udp_timeout;

        // Amnezia (AmneziaWG) options. Mirrors the amnezia_wg object of the
        // sing-box wireguard endpoint. jc/jmin/jmax and s1-s4 are integers,
        // h1-h4 (magic headers) and i1-i5 (signature packets) are passed
        // through verbatim as strings.
        bool enable_amnezia = false;
        int jc = 0;
        int jmin = 0;
        int jmax = 0;
        int s1 = 0;
        int s2 = 0;
        int s3 = 0;
        int s4 = 0;
        QString h1;
        QString h2;
        QString h3;
        QString h4;
        QString i1;
        QString i2;
        QString i3;
        QString i4;
        QString i5;

        // baseConfig overrides
        bool ParseFromLink(const QString& link) override;
        bool ParseFromJson(const QJsonObject& object) override;
        QString ExportToLink() override;
        QJsonObject ExportToJson() override;
        BuildResult Build() override;

        void SetPort(int newPort) override;
        QString GetPort() override;
        void SetAddress(QString newAddr) override;
        QString GetAddress() override;
        QString DisplayAddress() override;
        QString DisplayType() override;
        bool IsEndpoint() override;

        private:
        QJsonObject AmneziaToJson();
        void AmneziaFromJson(const QJsonObject& object);
        void FixAddress();
    };
}


