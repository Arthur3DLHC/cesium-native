#include "Cesium3DTilesSelection/CreditSystem.h"
#include "Cesium3DTilesSelection/QuadtreeRasterOverlayTileProvider.h"
#include "Cesium3DTilesSelection/RasterOverlayTile.h"
#include "Cesium3DTilesSelection/WebMapTileServiceRasterOverlay.h"
#include "Cesium3DTilesSelection/TilesetExternals.h"
#include "Cesium3DTilesSelection/spdlog-cesium.h"
#include "Cesium3DTilesSelection/tinyxml-cesium.h"

#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumAsync/IAssetResponse.h>
#include <CesiumGeospatial/GlobeRectangle.h>
#include <CesiumGeospatial/WebMercatorProjection.h>
#include <CesiumUtility/Uri.h>

#include <cstddef>
#include <iostream>

using namespace CesiumAsync;
using namespace CesiumUtility;

namespace Cesium3DTilesSelection {

class WebMapTileServiceTileProvider final
    : public QuadtreeRasterOverlayTileProvider {
public:
  WebMapTileServiceTileProvider(
      const IntrusivePointer<const RasterOverlay>& owner,
      const CesiumAsync::AsyncSystem& asyncSystem,
      const std::shared_ptr<IAssetAccessor>& pAssetAccessor,
      std::optional<Credit> credit,
      const std::shared_ptr<IPrepareRendererResources>&
          pPrepareRendererResources,
      const std::shared_ptr<spdlog::logger>& pLogger,
      const CesiumGeospatial::Projection& projection,
      const CesiumGeometry::QuadtreeTilingScheme& tilingScheme,
      const CesiumGeometry::Rectangle& coverageRectangle,
      const std::string& url,
      const std::string& version,
      const std::vector<IAssetAccessor::THeader>& headers,
      const std::string& layer,
      const std::string& style,
      const std::string& tileMatrixSet,
      const std::string& format,
      const std::string& tokenName,
      const std::string& tokenValue,
      uint32_t width,
      uint32_t height,
      uint32_t minimumLevel,
      uint32_t maximumLevel,
      int levelBias,
    // ������������������Ϊ WMTS �й̶��Ĺ淶
      // bool reverseX,
      // bool reverseY,
      const std::vector<std::string>& tileMatrixLabels,
      const std::vector<std::string>& subdomains)
      : QuadtreeRasterOverlayTileProvider(
            owner,
            asyncSystem,
            pAssetAccessor,
            credit,
            pPrepareRendererResources,
            pLogger,
            projection,
            tilingScheme,
            coverageRectangle,
            minimumLevel,
            maximumLevel,
            width,
            height),
        _url(url),
        _version(version),
        _headers(headers),
        _layer(layer),
        _style(style),
        _tileMatrixSet(tileMatrixSet),
        _format(format),
        _tokenName(tokenName),
        _tokenValue(tokenValue),
        _subdomains(subdomains),
        _tileMatrixLabels(tileMatrixLabels),
        _levelBias(levelBias)
        // _reverseX(reverseX),
        // _reverseY(reverseY)
        {
    // ���� url �ж��� KVP ���� RESTful ������
    // �ο� Cesium JS, Source\Scene\WebMapTileServiceImageryProvider.js �ж��߼�
    // ��� url ��û�г���������ţ���ֻ��һ�� {s} �����ţ�Ϊ����ģ�������������Ϊ�� KVP ��񣻷���Ϊ REST ���
    auto bracketCount = std::count(_url.begin(), _url.end(), '{');
    if (bracketCount == 0 ||
        (bracketCount == 1 && _url.find("{s}") != std::string::npos)) {
      _useKVP = true;
    } else {
      _useKVP = false;
    }
  }

  virtual ~WebMapTileServiceTileProvider() {}

protected:
  virtual CesiumAsync::Future<LoadedRasterOverlayImage> loadQuadtreeTileImage(
      const CesiumGeometry::QuadtreeTileID& tileID) const override {

    LoadTileImageFromUrlOptions options;
    options.rectangle = this->getTilingScheme().tileToRectangle(tileID);
    options.moreDetailAvailable = tileID.level < this->getMaximumLevel();

    // ���ʹ���� tile matrix label ����û���ṩ�㹻����Ŀ�����ؼ���ʧ�ܡ�
    if (_tileMatrixLabels.size() > 0 && _tileMatrixLabels.size() <= tileID.level) {
      return this->getAsyncSystem()
          .createResolvedFuture<LoadedRasterOverlayImage>(
              {std::nullopt,
               options.rectangle,
               {},
               {"Failed to load image from TMS."},
               {},
               options.moreDetailAvailable});
    }

    // ��γ��ͶӰ�µ� tileID ��Ҫ���⴦��
    // fix me: ����һ�� CesiumJS �� WMTS imagery provider ����ô�����
    uint32_t level = tileID.level + _levelBias;
    //if (this->getProjection().index() == 0) {
    //  level++;
    //}

    // WMTS ��Ҫ�� y ��ȡ��
    // fix me: ����һ�� CesiumJS �� WMTS imagery provider ����ô�����
    // uint32_t y = (1 << tileID.level) - 1 - tileID.y;
    // uint32_t y = getTilingScheme().getNumberOfYTilesAtLevel(tileID.level) - tileID.y - 1;
    uint32_t x = tileID.x;
    uint32_t y = tileID.y;

    //if (_reverseX) {
    //  // fix me: ����Ӧ��ʹ��ԭʼ level�����Ǽ��� bias ֮��� level����Ҫ���Լ���
    //  x = getTilingScheme().getNumberOfXTilesAtLevel(tileID.level) - x - 1;
    //}
    // if (_reverseY) {
      // fix me: ����Ӧ��ʹ��ԭʼ level�����Ǽ��� bias ֮���
      // level����Ҫ���Լ���

      // ���� WMTS �淶���� y ������ TMS �� Cesium ��Ĭ�Ϸ����෴��������Ҫȡ��
      y = getTilingScheme().getNumberOfYTilesAtLevel(tileID.level) - y - 1;
    // }


    std::string url;
    if (_useKVP) {
      // Ҳ��Ҫ�滻 {s} ģ�����
      std::string baseUrl = CesiumUtility::Uri::substituteTemplateParameters(
          _url,
          [this, &tileID](const std::string& key) {
            if (key == "s") {
              // ���ָ����{s}ģ��������� subdomains �����б�����Ԫ��
              assert(_subdomains.size() > 0);
              const size_t subdomainIndex =
                  (tileID.level + tileID.x + tileID.y) % _subdomains.size();
              return _subdomains[subdomainIndex];
            } else {
              return key;
            }
          });

      // KVP �����ã�ƴ�Ӳ�����ֵ���б�
      std::string requestParams =
          "?service=wmts&request=gettile&version=" + _version +
          "&layer=" + _layer + "&style=" + _style +
          "&tilematrixset=" + _tileMatrixSet + "&format=" + _format +
          "&tilematrix=" +
          (_tileMatrixLabels.size() > 0 ? _tileMatrixLabels[level]
                                        : std::to_string(level)) +
          "&tilerow=" + std::to_string(y) +
          "&tilecol=" + std::to_string(x);

      if (!_tokenName.empty() && !_tokenValue.empty()) {
        requestParams = requestParams + "&" + _tokenName + "=" + _tokenValue;
      }

      url = CesiumUtility::Uri::resolve(baseUrl, requestParams, true);
    } else {
      // REST �����ã��滻�ַ���ģ�����
      url = CesiumUtility::Uri::substituteTemplateParameters(
          _url,
          [this, &tileID, level, x, y](const std::string& key) {
            if (key == "s") {
              // ���ָ����{s}ģ��������� subdomains �����б�����Ԫ��
              assert(_subdomains.size() > 0);
              const size_t subdomainIndex =
                  (tileID.level + tileID.x + tileID.y) % _subdomains.size();
              return _subdomains[subdomainIndex];
            } else if (key == "Layer" || key == "layer") {
              return _layer;
            } 
            else if (key == "Style" || key == "style") {
              return _style;
            } /* else if (key == "Format" || key == "format") {
              // REST ���û�б�Ҫ�� {format} ģ���������Ϊ����ֱ�Ӱ�����ֵд��ģ���ַ�����
              return _format;
            } */
            else if (key == "TileMatrixSet" || key == "tilematrixset") {
              return _tileMatrixSet;
            }
            else if (key == "TileMatrix" || key == "tilematrix") {
              return _tileMatrixLabels.size() > 0 ? _tileMatrixLabels[level]
                                                  : std::to_string(level);
            } else if (key == "TileRow" || key == "tilerow") {
              return std::to_string(y);
            } else if (key == "TileCol" || key == "tilecol") {
              return std::to_string(x);
            } else {
              return key;
            }
          });
    }

		// ����
		//std::string tk = "9084ac98a18afab0293b5ce547b0fcac";

		//std::string url = CesiumUtility::Uri::resolve(
		//	this->_url,
		//	"?service=wmts&request=gettile&version=1.0.0&layer=img&style=default&tilematrixset=w&format=tiles&tilematrix=" +
		//	std::to_string(tileID.level) + "&tilerow=" + std::to_string(y) + "&tilecol=" + std::to_string(tileID.x) + "&tk=" + tk,
		//	true);

		// ���԰�������
	  //std::string url = CesiumUtility::Uri::resolve(
   //     this->_url,
   //     "WMTS/tile/1.0.0/Pub_BaseMapEng_LightGray_WM/default/GoogleMapsCompatible/" +
   //     std::to_string(tileID.level) + "/" + std::to_string(y) + "/" + std::to_string(tileID.x) + ".jpg",
   //     true);


    // std::cout << url << std::endl;
    // ::OuputDebugString(url.c_str());
    //printf(url.c_str());
    //printf("\n");


    return this->loadTileImageFromUrl(url, this->_headers, std::move(options));
  }

private:
  std::string _url;
  std::string _version;
  std::vector<IAssetAccessor::THeader> _headers;
  std::string _layer;
  std::string _style;
  std::string _tileMatrixSet;
  std::string _format;
  std::string _tokenName;
  std::string _tokenValue;


  // ������������������Ϊ WMTS �й̶��Ĺ淶
  // bool _reverseX = false;
  // bool _reverseY = false;
  int _levelBias = 0;
  bool _useKVP = false;
  std::vector<std::string> _subdomains;
  std::vector<std::string> _tileMatrixLabels;
};

WebMapTileServiceRasterOverlay::WebMapTileServiceRasterOverlay(
    const std::string& name,
    const std::string& url,
    const std::vector<IAssetAccessor::THeader>& headers,
    const WebMapTileServiceRasterOverlayOptions& tmsOptions,
    const RasterOverlayOptions& overlayOptions)
    : RasterOverlay(name, overlayOptions),
      _url(url),
      _headers(headers),
      _options(tmsOptions) {}

WebMapTileServiceRasterOverlay::~WebMapTileServiceRasterOverlay() {}

Future<RasterOverlay::CreateTileProviderResult>
    WebMapTileServiceRasterOverlay::createTileProvider(
    const CesiumAsync::AsyncSystem& asyncSystem,
    const std::shared_ptr<CesiumAsync::IAssetAccessor>& pAssetAccessor,
    const std::shared_ptr<CreditSystem>& pCreditSystem,
    const std::shared_ptr<IPrepareRendererResources>& pPrepareRendererResources,
    const std::shared_ptr<spdlog::logger>& pLogger,
    CesiumUtility::IntrusivePointer<const RasterOverlay> pOwner) const {

    const std::optional<Credit> credit =
      this->_options.credit ? std::make_optional(pCreditSystem->createCredit(
                                  this->_options.credit.value()))
                            : std::nullopt;

  pOwner = pOwner ? pOwner : this;

  // ���ԣ����Բ� Get Capability��ֱ�ӷ���һ�� Resolved ����
  // Ĭ��ʹ�þ�γ��ͶӰ��
  CesiumGeospatial::Projection projection =
      CesiumGeospatial::GeographicProjection();
  // CesiumGeospatial::Projection projection =
  // CesiumGeospatial::WebMercatorProjection();

  // Ĭ��ȫ��γ����Ƭ��Χ
  CesiumGeospatial::GlobeRectangle tilingSchemeRectangle =
      CesiumGeospatial::GeographicProjection::MAXIMUM_GLOBE_RECTANGLE;
  // CesiumGeospatial::GlobeRectangle tilingSchemeRectangle =
  // CesiumGeospatial::WebMercatorProjection::MAXIMUM_GLOBE_RECTANGLE;

  // ��γ����Ƭ���ڵ���Ƭ����Ϊ 2; Web ī���еĸ��ڵ���Ƭ����Ϊ 1
  // Ĭ��ʹ��γ����Ƭ
  uint32_t rootTilesX = 2;

  if (_options.projection) {
    projection = _options.projection.value();
    // ��Ҫ����ͶӰ��ʽȷ�� tilingSchemeRectangle �� rootTilesX
    // CesiumGeospatial::Projection ��һ��
    // std::variant����ģ�������������Ϊ GeographicProjection = 0,
    // WebMercatorProjection = 1 ��ע�⡿����Ժ� Cesium �޸��˴�
    // variant �Ĳ���ģ�嶨�壬����ҲҪ��Ӧ�޸ģ�

    // ������ Cesium JS Source\Core\GeographicTilingScheme.js ��
    // Source\Core\WebMercatorTilingScheme.js
    // �ıȶԣ�ȷ���˸��ڵ����Ƭ��������
    if (projection.index() == 0) {
      // ��������ϵ
      tilingSchemeRectangle =
          CesiumGeospatial::GeographicProjection::MAXIMUM_GLOBE_RECTANGLE;
      rootTilesX = 2;
    } else if (projection.index() == 1) {
      // Web ī��������ϵ
      tilingSchemeRectangle =
          CesiumGeospatial::WebMercatorProjection::MAXIMUM_GLOBE_RECTANGLE;
      rootTilesX = 1;
    }
  }

  CesiumGeometry::Rectangle coverageRectangle =
      _options.coverageRectangle.value_or(
          projectRectangleSimple(projection, tilingSchemeRectangle));

  CesiumGeometry::QuadtreeTilingScheme tilingScheme =
      _options.tilingScheme.value_or(CesiumGeometry::QuadtreeTilingScheme(
          projectRectangleSimple(projection, tilingSchemeRectangle),
          rootTilesX,
          1));

  // std::string url = this->_url;
  // std::vector<CesiumAsync::IAssetAccessor::THeader> headers =
  //    this->_headers;
  // std::string fileExtension =
  // options.fileExtension.value_or("jpg");
  std::string fileEextension = "";
  uint32_t tileWidth = _options.tileWidth.value_or(256);
  uint32_t tileHeight = _options.tileHeight.value_or(256);
  uint32_t minimumLevel = _options.minimumLevel.value_or(0);
  uint32_t maximumLevel = _options.maximumLevel.value_or(25);

  // δ�ṩ�����������ʹ��Ĭ��
  std::string version = _options.version.value_or("1.0.0");
  std::string style = _options.style.value_or("default");
  std::string format = _options.format.value_or("tiles");

  if (version.empty())
    version = "1.0.0";
  if (style.empty())
    style = "default";
  if (format.empty())
    format = "tiles";

  return asyncSystem
      .createResolvedFuture<RasterOverlay::CreateTileProviderResult>(
          IntrusivePointer<WebMapTileServiceTileProvider>(
              new WebMapTileServiceTileProvider(
                  pOwner,
                  asyncSystem,
                  pAssetAccessor,
                  credit,
                  pPrepareRendererResources,
                  pLogger,
                  projection,
                  tilingScheme,
                  coverageRectangle,
                  _url,
                  version,
                  _headers,
                  _options.layer.value_or("img"),
                  style,
                  _options.tileMatrixSet.value_or("c"),
                  format,
                  _options.tokenName.value_or(""),
                  _options.tokenValue.value_or(""),
                  tileWidth,
                  tileHeight,
                  minimumLevel,
                  maximumLevel,
                  _options.levelBias.value_or(0),
                  // _options.reverseX.value_or(false),
                  // _options.reverseY.value_or(false),
                  _options.tileMatrixLabels,
                  _options.subdomains)));


  /*

  // �����Ϊ��� WMTS Ability xml ����
  std::string xmlUrl =
      CesiumUtility::Uri::resolve(this->_url, "?service=wmts&request=GetCapabilities");



  return pAssetAccessor->get(asyncSystem, xmlUrl, this->_headers)
      .thenInWorkerThread(
          [pOwner,
           asyncSystem,
           pAssetAccessor,
           credit,
           pPrepareRendererResources,
           pLogger,
           options = this->_options,
           url = this->_url,
           headers =
               this->_headers](std::shared_ptr<IAssetRequest>& pRequest)
              -> CreateTileProviderResult {
            const IAssetResponse* pResponse = pRequest->response();
            if (!pResponse) {
              //return nonstd::make_unexpected(RasterOverlayLoadFailureDetails{
              //    RasterOverlayLoadType::TileProvider,
              //    std::move(pRequest),
              //    "No response received when getting WMTS capabilities."});
            }

            // Ĭ��ʹ�þ�γ��ͶӰ��
            CesiumGeospatial::Projection projection =
                CesiumGeospatial::GeographicProjection();
            // CesiumGeospatial::Projection projection =
            // CesiumGeospatial::WebMercatorProjection();

            // Ĭ��ȫ��γ����Ƭ��Χ
            CesiumGeospatial::GlobeRectangle tilingSchemeRectangle =
                CesiumGeospatial::GeographicProjection::MAXIMUM_GLOBE_RECTANGLE;
            // CesiumGeospatial::GlobeRectangle tilingSchemeRectangle =
            // CesiumGeospatial::WebMercatorProjection::MAXIMUM_GLOBE_RECTANGLE;

            // ��γ����Ƭ���ڵ���Ƭ����Ϊ 2; Web ī���еĸ��ڵ���Ƭ����Ϊ 1
            // Ĭ��ʹ��γ����Ƭ
            uint32_t rootTilesX = 2;

            if (options.projection) {
              projection = options.projection.value();
              // ��Ҫ����ͶӰ��ʽȷ�� tilingSchemeRectangle �� rootTilesX
              // CesiumGeospatial::Projection ��һ��
              // std::variant����ģ�������������Ϊ GeographicProjection = 0,
              // WebMercatorProjection = 1 ��ע�⡿����Ժ� Cesium �޸��˴�
              // variant �Ĳ���ģ�嶨�壬����ҲҪ��Ӧ�޸ģ�

              // ������ Cesium JS Source\Core\GeographicTilingScheme.js ��
              // Source\Core\WebMercatorTilingScheme.js
              // �ıȶԣ�ȷ���˸��ڵ����Ƭ��������
              if (projection.index() == 0) {
                // ��������ϵ
                tilingSchemeRectangle = CesiumGeospatial::GeographicProjection::
                    MAXIMUM_GLOBE_RECTANGLE;
                rootTilesX = 2;
              } else if (projection.index() == 1) {
                // Web ī��������ϵ
                tilingSchemeRectangle = CesiumGeospatial::
                    WebMercatorProjection::MAXIMUM_GLOBE_RECTANGLE;
                rootTilesX = 1;
              }
            }

            CesiumGeometry::Rectangle coverageRectangle =
                options.coverageRectangle.value_or(
                    projectRectangleSimple(projection, tilingSchemeRectangle));

            CesiumGeometry::QuadtreeTilingScheme tilingScheme =
                options.tilingScheme.value_or(
                    CesiumGeometry::QuadtreeTilingScheme(
                        projectRectangleSimple(
                            projection,
                            tilingSchemeRectangle),
                        rootTilesX,
                        1));

            // std::string url = this->_url;
            // std::vector<CesiumAsync::IAssetAccessor::THeader> headers =
            //    this->_headers;
            // std::string fileExtension =
            // options.fileExtension.value_or("jpg");
            std::string fileEextension = "";
            uint32_t tileWidth = options.tileWidth.value_or(256);
            uint32_t tileHeight = options.tileHeight.value_or(256);
            uint32_t minimumLevel = options.minimumLevel.value_or(0);
            uint32_t maximumLevel = options.maximumLevel.value_or(25);

            // δ�ṩ�����������ʹ��Ĭ��
            std::string version = options.version.value_or("1.0.0");
            std::string style = options.style.value_or("default");
            std::string format = options.format.value_or("tiles");

            if (version.empty())
              version = "1.0.0";
            if (style.empty())
              style = "default";
            if (format.empty())
              format = "tiles";

            return new WebMapTileServiceTileProvider(
                pOwner,
                asyncSystem,
                pAssetAccessor,
                credit,
                pPrepareRendererResources,
                pLogger,
                projection,
                tilingScheme,
                coverageRectangle,
                url,
                version,
                headers,
                options.layer.value_or("img"),
                style,
                options.tileMatrixSet.value_or("c"),
                format,
                options.tokenName.value_or(""),
                options.tokenValue.value_or(""),
                tileWidth,
                tileHeight,
                minimumLevel,
                maximumLevel,
                options.levelBias.value_or(0),
                options.reverseX.value_or(false),
                options.reverseY.value_or(false),
                options.tileMatrixLabels,
                options.subdomains);
          });
   */
}

} // namespace Cesium3DTilesSelection
