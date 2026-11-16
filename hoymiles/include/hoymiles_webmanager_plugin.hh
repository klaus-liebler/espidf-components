#pragma once
#include "hoymiles_types.hh"
#include "webmanager_interfaces.hh"
#include "wsprotocol_cpp/ws_protocol.hh"

// Haengt bewusst NUR von webmanager_interfaces.hh (iWebmanagerPlugin/iWebmanagerCallback) und dem
// generierten ws_protocol.hh ab -- keine Kopplung an sensact::hal/cApplications/NVS o.ae. Alle
// projektspezifischen/hartcodierten Werte (Seriennummern etc.) leben ausserhalb dieser Klasse, im
// jeweiligen board_init.hh.
namespace hoymiles
{
    class WebmanagerPlugin : public webmanager::iWebmanagerPlugin, public iHoymilesListener
    {
    private:
        webmanager::iWebmanagerCallback* callback{nullptr};
        HoymilesLiveData last{};

        static void Fill(const HoymilesLiveData& src, WsProtocol::hoymiles::DcChannel* dstDc, WsProtocol::hoymiles::AcPhase& dstAc, WsProtocol::hoymiles::InverterTotals& dstTotals)
        {
            for (int i = 0; i < DC_CHANNEL_COUNT; i++)
            {
                dstDc[i].udc = src.Dc[i].UdcV;
                dstDc[i].idc = src.Dc[i].IdcA;
                dstDc[i].pdc = src.Dc[i].PdcW;
                dstDc[i].yieldDay = src.Dc[i].YieldDayWh;
                dstDc[i].yieldTotal = src.Dc[i].YieldTotalKwh;
            }
            dstAc.uac = src.Ac.UacV;
            dstAc.iac = src.Ac.IacA;
            dstAc.pac = src.Ac.PacW;
            dstAc.reactivePower = src.Ac.ReactivePowerVar;
            dstAc.frequency = src.Ac.FrequencyHz;
            dstAc.powerFactor = src.Ac.PowerFactor;

            dstTotals.temperatureC = src.Totals.TemperatureC;
            dstTotals.eventLogCount = src.Totals.EventLogCount;
            dstTotals.yieldDay = src.Totals.YieldDayWh;
            dstTotals.yieldTotal = src.Totals.YieldTotalKwh;
            dstTotals.pdc = src.Totals.PdcW;
            dstTotals.efficiencyPct = src.Totals.EfficiencyPct;
        }

    public:
        void OnBegin(webmanager::iWebmanagerCallback* cb) override { this->callback = cb; }
        void OnWifiConnect(webmanager::iWebmanagerCallback* cb) override { this->callback = cb; }
        void OnWifiDisconnect(webmanager::iWebmanagerCallback* cb) override { this->callback = cb; }
        void OnTimeUpdate(webmanager::iWebmanagerCallback* cb) override { this->callback = cb; }

        // Server-Push nach jedem Poll-Zyklus des hoymiles::Manager (auch bei Fehlschlag, dann mit
        // den zuletzt bekannten guten Werten und Reachable=false -- s. Manager::Task()).
        void OnLiveDataUpdated(const HoymilesLiveData& data) override
        {
            this->last = data;
            if (!callback) return;

            WsProtocol::hoymiles::NotifyLiveData::Payload p{};
            p.reachable = data.Reachable;
            p.producing = data.Producing;
            p.dataAgeMs = data.DataAgeMs;
            Fill(data, p.dc, p.ac, p.totals);

            uint8_t buf[WsProtocol::hoymiles::NotifyLiveData::NotifyLiveData_MAX_SIZE];
            size_t len = WsProtocol::hoymiles::NotifyLiveData::Encode(p, buf, sizeof(buf));
            if (len > 0) (void)callback->SendRawAsync(buf, len);
        }

        webmanager::eMessageReceiverResult ProvideWebsocketMessage(webmanager::iWebmanagerCallback* callback, WsProtocol::Responder& reply, const uint8_t* frame, size_t frameLen) override
        {
            namespace hm = WsProtocol::hoymiles;
            this->callback = callback;
            if (reply.Request().namespaceId != hm::NAMESPACE_ID || reply.Request().messageTypeId != hm::RequestLiveData::TYPE_ID)
                return webmanager::eMessageReceiverResult::NOT_FOR_ME;  // another type of hoymiles: UnknownMessage
            return webmanager::Handle(reply, frame, frameLen, hm::RequestLiveData::Decode,
                                      [this](WsProtocol::Responder& r, const hm::RequestLiveData::Payload&) {
                                          // the last cached snapshot - the page need not wait up to
                                          // pollIntervalMs for the next notify push
                                          hm::ResponseLiveData::Payload resp{};
                                          resp.reachable = last.Reachable;
                                          resp.producing = last.Producing;
                                          resp.dataAgeMs = last.DataAgeMs;
                                          Fill(last, resp.dc, resp.ac, resp.totals);
                                          r.Send(hm::ResponseLiveData::Encode, resp);
                                          return webmanager::eMessageReceiverResult::OK;
                                      });
        }
    };
}
