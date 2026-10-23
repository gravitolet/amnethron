#pragma once

#include <QWidget>
#include <QListWidgetItem>
#include <QDialog>
#include <QEvent>
#include <QShortcut>

#include "3rdparty/qv2ray/v2/ui/QvAutoCompleteTextEdit.hpp"
#include "ui_RouteItem.h"
#include "include/database/entities/RouteProfile.h"

class QListWidget;

class RouteItem : public QDialog {
    Q_OBJECT

public:
    explicit RouteItem(QWidget *parent = nullptr, const std::shared_ptr<Configs::RouteProfile>& routeChain = nullptr);
    ~RouteItem() override;

    std::shared_ptr<Configs::RouteProfile> chain;
signals:
    void settingsChanged(std::shared_ptr<Configs::RouteProfile> routingChain);

private:
    Ui::RouteItem *ui;
    int currentIndex = -1;

    int lastNum = 0;


    QShortcut* deleteShortcut;

    QStringList outbounds;

    std::map<int,int> outboundMap;

    AutoCompleteTextEdit* simpleDirect;

    AutoCompleteTextEdit* simpleBlock;

    AutoCompleteTextEdit* simpleProxy;

    QListWidget* ruleAttrPlusList = nullptr;

    void ensurePlusTabBuiltOnce();

    void removeAllAttributeTabsExceptPlus();

    void syncPlusListCheckStatesFromRule();

    void persistCurrentRuleAttrTabLabel();

    void applyStoredRuleAttrTabSelection();

    void syncRuleActionCombo();

    void rebuildRuleAttributeTabs();

    [[nodiscard]] QWidget* makeAttributeEditorPage(const QString& attr);

    void updateRuleSection();

    void updateRulePreview();

    void updateRouteItemsView();

    void applyAttributeVisibilityChange(const QString& attr, bool visible);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void accept() override;

    void on_new_route_item_clicked();
    void on_moveup_route_item_clicked();
    void on_movedown_route_item_clicked();
    void on_delete_route_item_clicked();
};
