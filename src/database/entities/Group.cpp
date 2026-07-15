#include <include/database/entities/Group.h>

#include <algorithm>

#include "include/database/ProfilesRepo.h"
#include "include/global/Configs.hpp"

namespace Configs
{
    void Group::clearCalculatedColumnWidth() {
        calculated_column_width.clear();
    }

    QList<int> Group::Profiles() const {
        return profiles;
    }

    double bitrateToBps(const QString& str)
    {
        if (str.endsWith("Gbps", Qt::CaseInsensitive)) {
            double val = str.left(str.size() - 4).toDouble();
            return val * 1e9;
        }
        if (str.endsWith("Mbps", Qt::CaseInsensitive)) {
            double val = str.left(str.size() - 4).toDouble();
            return val * 1e6;
        }
        if (str.endsWith("Kbps", Qt::CaseInsensitive)) {
            double val = str.left(str.size() - 4).toDouble();
            return val * 1e3;
        }
        if (str == "N/A") return -1;
        return 0.0;
    }

    bool Group::SortProfiles(GroupSortAction sortAction, bool waitForLock) {
        if (waitForLock) {
            mutex.lock();
        } else if (!mutex.tryLock()) {
            return false;
        }
        sort_method = sortAction.method;
        sort_descending = sortAction.descending;
        auto allProfs = dataManager->profilesRepo->GetProfileBatch(profiles); // to warm up the cache
        switch (sortAction.method) {
            case GroupSortMethod::Raw: {
                break;
            }
            case GroupSortMethod::ById: {
                break;
            }
            case GroupSortMethod::ByAddress:
            case GroupSortMethod::ByName:
            case GroupSortMethod::ByTestResult:
            case GroupSortMethod::ByTraffic:
            case GroupSortMethod::ByType: {
                auto get_speed_product_for_sort = [](const std::shared_ptr<Profile>& prof) {
                    const long double dl = std::max(
                        0.0L, static_cast<long double>(bitrateToBps(prof->dl_speed)));
                    const long double ul = std::max(
                        0.0L, static_cast<long double>(bitrateToBps(prof->ul_speed)));
                    return dl * ul;
                };
                std::stable_sort(profiles.begin(), profiles.end(),
                                 [&](int a, int b) {
                                      auto profA = dataManager->profilesRepo->GetProfile(a);
                                      auto profB = dataManager->profilesRepo->GetProfile(b);
                                      if (profA == nullptr || profB == nullptr) return profA != nullptr;
                                      QString ms_a;
                                      QString ms_b;
                                      if (sortAction.method == GroupSortMethod::ByType) {
                                          ms_a = profA->outbound->DisplayType();
                                          ms_b = profB->outbound->DisplayType();
                                      } else if (sortAction.method == GroupSortMethod::ByName) {
                                          ms_a = profA->outbound->name;
                                          ms_b = profB->outbound->name;
                                      } else if (sortAction.method == GroupSortMethod::ByAddress) {
                                          ms_a = profA->outbound->DisplayAddress();
                                          ms_b = profB->outbound->DisplayAddress();
                                      } else if (sortAction.method == GroupSortMethod::ByTestResult) {
                                          if (test_sort_by == testBy::speedProduct) {
                                              const long double productA = get_speed_product_for_sort(profA);
                                              const long double productB = get_speed_product_for_sort(profB);
                                              return sortAction.descending ? productA > productB : productA < productB;
                                          }
                                          if (test_sort_by == testBy::dlSpeed) {
                                              return sortAction.descending ? bitrateToBps(profA->dl_speed) > bitrateToBps(profB->dl_speed) : bitrateToBps(profA->dl_speed) < bitrateToBps(profB->dl_speed);
                                          }
                                          if (test_sort_by == testBy::ulSpeed) {
                                              return sortAction.descending ? bitrateToBps(profA->ul_speed) > bitrateToBps(profB->ul_speed) : bitrateToBps(profA->ul_speed) < bitrateToBps(profB->ul_speed);
                                          }
                                          if (test_sort_by == testBy::ipOut) {
                                              return sortAction.descending ? profA->ip_out > profB->ip_out : profA->ip_out < profB->ip_out;
                                          }
                                      } else if (sortAction.method == GroupSortMethod::ByTraffic) {
                                          if (traffic_sort_by == trafficBy::total) {
                                              auto totalA = profA->traffic_downlink + profA->traffic_uplink;
                                              auto totalB = profB->traffic_downlink + profB->traffic_uplink;
                                              return sortAction.descending ? totalA > totalB  : totalA < totalB;
                                          }
                                          if (traffic_sort_by == trafficBy::dl) {
                                              return sortAction.descending ? profA->traffic_downlink > profB->traffic_downlink : profA->traffic_downlink < profB->traffic_downlink;
                                          }
                                          if (traffic_sort_by == trafficBy::ul) {
                                              return sortAction.descending ? profA->traffic_uplink > profB->traffic_uplink : profA->traffic_uplink < profB->traffic_uplink;
                                          }
                                      }
                                      return sortAction.descending ? ms_a > ms_b : ms_a < ms_b;
                                 });
                break;
            }
        }
        mutex.unlock();
        return true;
    }

    bool Group::AddProfile(int ID)
    {
        QMutexLocker locker(&mutex);
        if (HasProfile(ID))
        {
            return false;
        }
        profiles.append(ID);
        auto_switch_profiles.insert(ID);
        return true;
    }

    bool Group::AddProfileBatch(const QList<int>& IDs) {
        QSet<int> currentProfiles;
        for (const auto& profileID : profiles) {
            currentProfiles.insert(profileID);
        }
        QMutexLocker locker(&mutex);
        for (auto profileID : IDs) {
            if (!currentProfiles.contains(profileID)) {
                profiles.append(profileID);
                auto_switch_profiles.insert(profileID);
            }
        }
        return true;
    }

    bool Group::RemoveProfile(int ID)
    {
        QMutexLocker locker(&mutex);
        if (!HasProfile(ID)) return false;
        profiles.removeAll(ID);
        auto_switch_profiles.remove(ID);
        return true;
    }

    bool Group::RemoveProfileBatch(const QList<int>& IDs) {
        QSet<int> toDel;
        for (auto ID : IDs) {
            toDel.insert(ID);
        }
        QList<int> newIDs;
        QMutexLocker locker(&mutex);
        for (auto inID : profiles) {
            if (!toDel.contains(inID)) {
                newIDs.append(inID);
            }
        }
        profiles = newIDs;
        for (auto it = auto_switch_profiles.begin(); it != auto_switch_profiles.end();) {
            if (!profiles.contains(*it)) it = auto_switch_profiles.erase(it);
            else ++it;
        }
        return true;
    }

    bool Group::SwapProfiles(int idx1, int idx2)
    {
        QMutexLocker locker(&mutex);
        if (profiles.size() <= idx1 || profiles.size() <= idx2) return false;
        profiles.swapItemsAt(idx1, idx2);
        return true;
    }

    bool Group::EmplaceProfile(int idx, int newIdx)
    {
        QMutexLocker locker(&mutex);
        if (profiles.size() <= idx || profiles.size() <= newIdx) return false;
        profiles.insert(newIdx+1, profiles[idx]);
        if (idx < newIdx) profiles.remove(idx);
        else profiles.remove(idx+1);
        return true;
    }

    bool Group::HasProfile(int ID) const
    {
        return profiles.contains(ID);
    }

    bool Group::IsAutoSwitchProfile(int ID) const
    {
        QMutexLocker locker(&mutex);
        return auto_switch_profiles.contains(ID);
    }

    void Group::SetAutoSwitchProfile(int ID, bool selected)
    {
        QMutexLocker locker(&mutex);
        if (!profiles.contains(ID)) return;
        if (selected) auto_switch_profiles.insert(ID);
        else auto_switch_profiles.remove(ID);
    }

    void Group::SetAllAutoSwitchProfiles(bool selected)
    {
        QMutexLocker locker(&mutex);
        auto_switch_profiles.clear();
        if (!selected) return;
        for (int ID : profiles) auto_switch_profiles.insert(ID);
    }

    int Group::AutoSwitchProfileCount() const
    {
        QMutexLocker locker(&mutex);
        int count = 0;
        for (int ID : profiles) {
            if (auto_switch_profiles.contains(ID)) ++count;
        }
        return count;
    }
}
