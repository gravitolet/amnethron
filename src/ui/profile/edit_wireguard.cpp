#include "include/ui/profile/edit_wireguard.h"

EditWireguard::EditWireguard(QWidget *parent) : QWidget(parent), ui(new Ui::EditWireguard) {
    ui->setupUi(this);
}

EditWireguard::~EditWireguard() {
    delete ui;
}

void EditWireguard::onStart(std::shared_ptr<Configs::Profile> _ent) {
    this->ent = _ent;
    auto outbound = this->ent->Wireguard();

#ifndef Q_OS_LINUX
    adjustSize();
#endif

    ui->private_key->setText(outbound->private_key);
    ui->public_key->setText(outbound->peer->public_key);
    ui->preshared_key->setText(outbound->peer->pre_shared_key);
    ui->reserved->setText(QListInt2QListString(outbound->peer->reserved).join(","));
    ui->persistent_keepalive->setText(Int2String(outbound->peer->persistent_keepalive));
    ui->mtu->setText(Int2String(outbound->mtu));
    ui->sys_ifc->setChecked(outbound->system);
    ui->local_addr->setText(outbound->address.join(","));
    ui->workers->setText(Int2String(outbound->worker_count));

    ui->enable_amnezia->setChecked(outbound->enable_amnezia);
    ui->jc->setText(Int2String(outbound->jc));
    ui->jmin->setText(Int2String(outbound->jmin));
    ui->jmax->setText(Int2String(outbound->jmax));
    ui->s1->setText(Int2String(outbound->s1));
    ui->s2->setText(Int2String(outbound->s2));
    ui->s3->setText(Int2String(outbound->s3));
    ui->s4->setText(Int2String(outbound->s4));
    ui->h1->setText(outbound->h1);
    ui->h2->setText(outbound->h2);
    ui->h3->setText(outbound->h3);
    ui->h4->setText(outbound->h4);
    ui->i1->setText(outbound->i1);
    ui->i2->setText(outbound->i2);
    ui->i3->setText(outbound->i3);
    ui->i4->setText(outbound->i4);
    ui->i5->setText(outbound->i5);
    ui->hpk->setText(outbound->header_protection_key);
    ui->cpa->setText(outbound->content_padding_addition > 0 ? Int2String(outbound->content_padding_addition) : "");
    ui->random_trailers->setText(outbound->random_trailers > 0 ? Int2String(outbound->random_trailers) : "");
    ui->disable_cookies->setChecked(outbound->disable_cookies);
}

bool EditWireguard::onEnd() {
    auto outbound = this->ent->Wireguard();

    outbound->private_key = ui->private_key->text();
    outbound->peer->public_key = ui->public_key->text();
    outbound->peer->pre_shared_key = ui->preshared_key->text();
    auto rawReserved = ui->reserved->text();
    outbound->peer->reserved = {};
    for (const auto& item: rawReserved.split(",")) {
        if (item.trimmed().isEmpty()) continue;
        outbound->peer->reserved += item.trimmed().toInt();
    }
    outbound->peer->persistent_keepalive = ui->persistent_keepalive->text().trimmed().toInt();
    outbound->mtu = ui->mtu->text().toInt();
    outbound->system = ui->sys_ifc->isChecked();
    outbound->address = ui->local_addr->text().replace(" ", "").split(",");
    outbound->worker_count = ui->workers->text().toInt();

    outbound->enable_amnezia = ui->enable_amnezia->isChecked();
    outbound->jc = ui->jc->text().toInt();
    outbound->jmin = ui->jmin->text().toInt();
    outbound->jmax = ui->jmax->text().toInt();
    outbound->s1 = ui->s1->text().toInt();
    outbound->s2 = ui->s2->text().toInt();
    outbound->s3 = ui->s3->text().toInt();
    outbound->s4 = ui->s4->text().toInt();
    outbound->h1 = ui->h1->text();
    outbound->h2 = ui->h2->text();
    outbound->h3 = ui->h3->text();
    outbound->h4 = ui->h4->text();
    outbound->i1 = ui->i1->text();
    outbound->i2 = ui->i2->text();
    outbound->i3 = ui->i3->text();
    outbound->i4 = ui->i4->text();
    outbound->i5 = ui->i5->text();
    outbound->header_protection_key = ui->hpk->text().trimmed();
    outbound->content_padding_addition = ui->cpa->text().trimmed().toInt();
    outbound->random_trailers = ui->random_trailers->text().trimmed().toInt();
    outbound->disable_cookies = ui->disable_cookies->isChecked();

    return true;
}