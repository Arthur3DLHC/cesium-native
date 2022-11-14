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
      uint32_t maximumLevel)
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
        _tokenValue(tokenValue) {}

  virtual ~WebMapTileServiceTileProvider() {}

protected:
  virtual CesiumAsync::Future<LoadedRasterOverlayImage> loadQuadtreeTileImage(
      const CesiumGeometry::QuadtreeTileID& tileID) const override {

    // 经纬度投影下的 tileID 需要特殊处理
    uint32_t level = tileID.level;
    if (this->getProjection().index() == 0) {
      level++;
    }

    // WMTS 需要将 y 轴取反
    uint32_t y = (1 << tileID.level) - 1 - tileID.y;

    // 拼接参数

    std::string requestParams =
        "?service=wmts&request=gettile&version=" + _version +
        "&layer=" + _layer + "&style=" + _style +
        "&tilematrixset=" + _tileMatrixSet + "&format=" + _format +
        "&tilematrix=" + std::to_string(level) +
        "&tilerow=" + std::to_string(y) +
        "&tilecol=" + std::to_string(tileID.x);

    if (!_tokenName.empty() && !_tokenValue.empty()) {
      requestParams = requestParams + "&" + _tokenName + "=" + _tokenValue;
    }

    // TODO: 改为将 url 字符串视为模板？与 Cesium JS 保持一致
    // TODO: 支持 subdomains

    std::string url =
        CesiumUtility::Uri::resolve(this->_url, requestParams, true);

		// 调试
		//std::string tk = "9084ac98a18afab0293b5ce547b0fcac";

		//std::string url = CesiumUtility::Uri::resolve(
		//	this->_url,
		//	"?service=wmts&request=gettile&version=1.0.0&layer=img&style=default&tilematrixset=w&format=tiles&tilematrix=" +
		//	std::to_string(tileID.level) + "&tilerow=" + std::to_string(y) + "&tilecol=" + std::to_string(tileID.x) + "&tk=" + tk,
		//	true);

		// 测试阿布扎比
	  //std::string url = CesiumUtility::Uri::resolve(
   //     this->_url,
   //     "WMTS/tile/1.0.0/Pub_BaseMapEng_LightGray_WM/default/GoogleMapsCompatible/" +
   //     std::to_string(tileID.level) + "/" + std::to_string(y) + "/" + std::to_string(tileID.x) + ".jpg",
   //     true);


    // std::cout << url << std::endl;
    // ::OuputDebugString(url.c_str());
    printf(url.c_str());
    printf("\n");

    LoadTileImageFromUrlOptions options;
    options.rectangle = this->getTilingScheme().tileToRectangle(tileID);
    options.moreDetailAvailable = tileID.level < this->getMaximumLevel();
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

  // 这里改为获得 WMTS Ability xml 数据
  std::string xmlUrl =
      CesiumUtility::Uri::resolve(this->_url, "?service=wmts&request=GetCapabilities");

  pOwner = pOwner ? pOwner : this;

  const std::optional<Credit> credit =
      this->_options.credit ? std::make_optional(pCreditSystem->createCredit(
                                  this->_options.credit.value()))
                            : std::nullopt;

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
               this->_headers](const std::shared_ptr<IAssetRequest>& pRequest)
              -> CreateTileProviderResult {
            const IAssetResponse* pResponse = pRequest->response();
            if (!pResponse) {
              return nonstd::make_unexpected(RasterOverlayLoadFailureDetails{
                  RasterOverlayLoadType::TileProvider,
                  std::move(pRequest),
                  "No response received from Tile Map Service."});
            }

            // 默认使用经纬度投影？
            CesiumGeospatial::Projection projection =
                CesiumGeospatial::GeographicProjection();
            // CesiumGeospatial::Projection projection =
            // CesiumGeospatial::WebMercatorProjection();

            // 默认全球经纬度瓦片范围
            CesiumGeospatial::GlobeRectangle tilingSchemeRectangle =
                CesiumGeospatial::GeographicProjection::MAXIMUM_GLOBE_RECTANGLE;
            // CesiumGeospatial::GlobeRectangle tilingSchemeRectangle =
            // CesiumGeospatial::WebMercatorProjection::MAXIMUM_GLOBE_RECTANGLE;

            // 经纬度瓦片根节点瓦片列数为 2; Web 墨卡托的根节点瓦片列数为 1
            // 默认使用纬度瓦片
            uint32_t rootTilesX = 2;

            if (options.projection) {
              projection = options.projection.value();
              // 需要根据投影方式确定 tilingSchemeRectangle 和 rootTilesX
              // CesiumGeospatial::Projection 是一个
              // std::variant，其模板参数类型索引为 GeographicProjection = 0,
              // WebMercatorProjection = 1 【注意】如果以后 Cesium 修改了此
              // variant 的参数模板定义，这里也要相应修改！
              if (projection.index() == 0) {
                // 地理坐标系
                tilingSchemeRectangle = CesiumGeospatial::GeographicProjection::
                    MAXIMUM_GLOBE_RECTANGLE;
                rootTilesX = 2;
              } else if (projection.index() == 1) {
                // Web 墨卡托坐标系
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

            // 未提供的请求参数就使用默认
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
                maximumLevel);
          });
}

} // namespace Cesium3DTilesSelection
