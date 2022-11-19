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
    // 这两个参数废弃，因为 WMTS 有固定的规范
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
    // 根据 url 判断是 KVP 还是 RESTful 风格调用
    // 参考 Cesium JS, Source\Scene\WebMapTileServiceImageryProvider.js 判断逻辑
    // 如果 url 中没有出现左大括号，或只有一个 {s} 大括号（为子域模板参数），则认为是 KVP 风格；否则为 REST 风格。
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

    // 如果使用了 tile matrix label 但是没有提供足够的条目，返回加载失败。
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

    // 经纬度投影下的 tileID 需要特殊处理
    // fix me: 查阅一下 CesiumJS 的 WMTS imagery provider 是怎么处理的
    uint32_t level = tileID.level + _levelBias;
    //if (this->getProjection().index() == 0) {
    //  level++;
    //}

    // WMTS 需要将 y 轴取反
    // fix me: 查阅一下 CesiumJS 的 WMTS imagery provider 是怎么处理的
    // uint32_t y = (1 << tileID.level) - 1 - tileID.y;
    // uint32_t y = getTilingScheme().getNumberOfYTilesAtLevel(tileID.level) - tileID.y - 1;
    uint32_t x = tileID.x;
    uint32_t y = tileID.y;

    //if (_reverseX) {
    //  // fix me: 这里应该使用原始 level，还是加了 bias 之后的 level？需要调试检验
    //  x = getTilingScheme().getNumberOfXTilesAtLevel(tileID.level) - x - 1;
    //}
    // if (_reverseY) {
      // fix me: 这里应该使用原始 level，还是加了 bias 之后的
      // level？需要调试检验

      // 根据 WMTS 规范，其 y 坐标与 TMS 和 Cesium 的默认方向相反，所以需要取逆
      y = getTilingScheme().getNumberOfYTilesAtLevel(tileID.level) - y - 1;
    // }


    std::string url;
    if (_useKVP) {
      // 也需要替换 {s} 模板参数
      std::string baseUrl = CesiumUtility::Uri::substituteTemplateParameters(
          _url,
          [this, &tileID](const std::string& key) {
            if (key == "s") {
              // 如果指定了{s}模板参数，则 subdomains 数组中必须有元素
              assert(_subdomains.size() > 0);
              const size_t subdomainIndex =
                  (tileID.level + tileID.x + tileID.y) % _subdomains.size();
              return _subdomains[subdomainIndex];
            } else {
              return key;
            }
          });

      // KVP 风格调用，拼接参数键值对列表
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
      // REST 风格调用，替换字符串模板参数
      url = CesiumUtility::Uri::substituteTemplateParameters(
          _url,
          [this, &tileID, level, x, y](const std::string& key) {
            if (key == "s") {
              // 如果指定了{s}模板参数，则 subdomains 数组中必须有元素
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
              // REST 风格没有必要加 {format} 模板参数，因为可以直接把它的值写在模板字符串里
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


  // 这两个参数废弃，因为 WMTS 有固定的规范
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

  // 调试：尝试不 Get Capability，直接返回一个 Resolved 对象
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

  if (_options.projection) {
    projection = _options.projection.value();
    // 需要根据投影方式确定 tilingSchemeRectangle 和 rootTilesX
    // CesiumGeospatial::Projection 是一个
    // std::variant，其模板参数类型索引为 GeographicProjection = 0,
    // WebMercatorProjection = 1 【注意】如果以后 Cesium 修改了此
    // variant 的参数模板定义，这里也要相应修改！

    // 经过与 Cesium JS Source\Core\GeographicTilingScheme.js 和
    // Source\Core\WebMercatorTilingScheme.js
    // 的比对，确定了根节点的瓦片行列数。
    if (projection.index() == 0) {
      // 地理坐标系
      tilingSchemeRectangle =
          CesiumGeospatial::GeographicProjection::MAXIMUM_GLOBE_RECTANGLE;
      rootTilesX = 2;
    } else if (projection.index() == 1) {
      // Web 墨卡托坐标系
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

  // 未提供的请求参数就使用默认
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

  // 这里改为获得 WMTS Ability xml 数据
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

              // 经过与 Cesium JS Source\Core\GeographicTilingScheme.js 和
              // Source\Core\WebMercatorTilingScheme.js
              // 的比对，确定了根节点的瓦片行列数。
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
