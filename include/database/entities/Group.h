#pragma once
#include <QList>
#include <QMutex>
#include <QSet>
#include <QString>

#include "include/ui/group/GroupSort.hpp"

namespace Configs
{
    enum class testBy : int {
        speedProduct = 0,
        dlSpeed,
        ulSpeed,
        ipOut
    };

    enum class testShowItems : int {
        all = 0,
        none,
        ipOnly,
        speedOnly
    };

    enum class trafficBy : int {
        total = 0,
        dl,
        ul
    };

    // Parse a human bitrate string ("12.34Mbps", "1.5Gbps", "999.00Kbps") into bits/sec.
    // Returns -1 for "N/A" and 0 for empty/unknown, matching the sort/selection callers.
    double bitrateToBps(const QString& str);

    class Group {
    public:
        mutable QMutex mutex;
        int id = -1;
        bool archive = false;
        bool skip_auto_update = false;
        bool auto_clear_unavailable = false;
        QString name = "";
        QString url = "";
        QString info = "";
        qint64 sub_last_update = 0;
        int front_proxy_id = -1;
        int landing_proxy_id = -1;

        // list ui
        QList<int> column_width;
        QList<int> calculated_column_width; // memory only, no need to save to db
        QList<int> profiles;
        int scroll_last_profile = -1;
        testBy test_sort_by = testBy::speedProduct;
        trafficBy traffic_sort_by = trafficBy::total;
        testShowItems test_items_to_show = testShowItems::all;
        GroupSortMethod::GroupSortMethod sort_method = GroupSortMethod::Raw;
        bool sort_descending = false;
        QSet<int> auto_switch_profiles;
        QList<std::pair<int, int>> selectedProfilesIdIdxPairs; // memory only, no need to save to db, pairs of (profileID, index)

        Group() = default;

        void clearCalculatedColumnWidth();

        [[nodiscard]] QList<int> Profiles() const;

        // Auto-switch profile IDs in the same order as the group profile list.
        [[nodiscard]] QList<int> AutoSwitchProfiles() const;

        bool SortProfiles(GroupSortAction method, bool waitForLock = false);

        [[nodiscard]] bool IsAutoSwitchProfile(int ID) const;

        void SetAutoSwitchProfile(int ID, bool selected);

        void SetAllAutoSwitchProfiles(bool selected);

        [[nodiscard]] int AutoSwitchProfileCount() const;

        bool RemoveProfile(int ID);

        bool RemoveProfileBatch(const QList<int>& IDs);

        bool AddProfile(int ID);

        bool AddProfileBatch(const QList<int>& IDs);

        bool SwapProfiles(int idx1, int idx2);

        bool EmplaceProfile(int idx, int newIdx);

        [[nodiscard]] bool HasProfile(int ID) const;
    };
}// namespace Configs
