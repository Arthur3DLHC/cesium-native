#include "Cesium3DTilesSelection/CreditSystem.h"
#include "Cesium3DTilesSelection/QuadtreeRasterOverlayTileProvider.h"
#include "Cesium3DTilesSelection/RasterOverlayTile.h"
#include "Cesium3DTilesSelection/UrlTemplateRasterOverlay.h"
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
using namespace CesiumUtility;


// #include <windows.h>

using namespace CesiumAsync;

namespace Cesium3DTilesSelection {

class UrlTemplateTileProvider final
    : public QuadtreeRasterOverlayTileProvider {
public:
  UrlTemplateTileProvider(
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
      const std::vector<IAssetAccessor::THeader>& headers,
      const std::string& fileExtension,
      uint32_t width,
      uint32_t height,
      uint32_t minimumLevel,
      uint32_t maximumLevel,
			int32_t levelBias,
			bool flipY)
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
        _headers(headers),
        _fileExtension(fileExtension),
				_flipY(flipY),
				_levelBias(levelBias){}

  virtual ~UrlTemplateTileProvider() {}

protected:
  virtual CesiumAsync::Future<LoadedRasterOverlayImage> loadQuadtreeTileImage(
      const CesiumGeometry::QuadtreeTileID& tileID) const override {

    // 判断是否需要将 y 轴取反
		// 此时不要加 level bias
		uint32_t y = tileID.y;
		if (_flipY) y = (1 << tileID.level) - 1 - tileID.y;

		// 替换 _url 中的 {x}, {y}, {z} 字符串

		std::string url = this->_url;
		size_t found = std::string::npos;
		if((found = url.find("{x}")) != std::string::npos){
			url.replace(found, 3, std::to_string(tileID.x));
		}
		if((found = url.find("{y}")) != std::string::npos) {
			url.replace(found, 3, std::to_string(y));
		}
		if((found = url.find("{z}")) != std::string::npos) {
			url.replace(found, 3, std::to_string(tileID.level + _levelBias));
		}

		// OutputDebugString(url.c_str());

	  //std::string url = CesiumUtility::Uri::resolve(
   //     this->_url,
   //     "WMTS/tile/1.0.0/Pub_BaseMapEng_LightGray_WM/default/GoogleMapsCompatible/" +
   //     std::to_string(tileID.level) + "/" + std::to_string(y) + "/" + std::to_string(tileID.x) + ".jpg",
   //     true);


    LoadTileImageFromUrlOptions options;
    options.rectangle = this->getTilingScheme().tileToRectangle(tileID);
    options.moreDetailAvailable = tileID.level < this->getMaximumLevel();
    return this->loadTileImageFromUrl(url, this->_headers, std::move(options));
  }

private:
  std::string _url;
  std::vector<IAssetAccessor::THeader> _headers;
  std::string _fileExtension;
	bool _flipY;
	int32_t _levelBias;
};

UrlTemplateRasterOverlay::UrlTemplateRasterOverlay(
    const std::string& name,
    const std::string& url,
    const std::vector<IAssetAccessor::THeader>& headers,
    const UrlTemplateRasterOverlayOptions& tmsOptions,
    const RasterOverlayOptions& overlayOptions)
    : RasterOverlay(name, overlayOptions),
      _url(url),
      _headers(headers),
      _options(tmsOptions) {}

UrlTemplateRasterOverlay::~UrlTemplateRasterOverlay() {}

/*
static std::optional<std::string> getAttributeString(
    const tinyxml2::XMLElement* pElement,
    const char* attributeName) {
  if (!pElement) {
    return std::nullopt;
  }

  const char* pAttrValue = pElement->Attribute(attributeName);
  if (!pAttrValue) {
    return std::nullopt;
  }

  return std::string(pAttrValue);
}

static std::optional<uint32_t> getAttributeUint32(
    const tinyxml2::XMLElement* pElement,
    const char* attributeName) {
  std::optional<std::string> s = getAttributeString(pElement, attributeName);
  if (s) {
    return std::stoul(s.value());
  }
  return std::nullopt;
}

static std::optional<double> getAttributeDouble(
    const tinyxml2::XMLElement* pElement,
    const char* attributeName) {
  std::optional<std::string> s = getAttributeString(pElement, attributeName);
  if (s) {
    return std::stod(s.value());
  }
  return std::nullopt;
}
*/

Future<RasterOverlay::CreateTileProviderResult>
UrlTemplateRasterOverlay::createTileProvider(
    const CesiumAsync::AsyncSystem& asyncSystem,
    const std::shared_ptr<CesiumAsync::IAssetAccessor>& pAssetAccessor,
    const std::shared_ptr<CreditSystem>& pCreditSystem,
    const std::shared_ptr<IPrepareRendererResources>& pPrepareRendererResources,
    const std::shared_ptr<spdlog::logger>& pLogger,
    CesiumUtility::IntrusivePointer<const RasterOverlay> pOwner) const {

  //std::string xmlUrl =
  //    CesiumUtility::Uri::resolve(this->_url, "?request=GetCapabilities&service=wmts");

  pOwner = pOwner ? pOwner : this;

  const std::optional<Credit> credit =
      this->_options.credit ? std::make_optional(pCreditSystem->createCredit(
                                  this->_options.credit.value()))
                            : std::nullopt;

	CesiumGeospatial::GlobeRectangle tilingSchemeRectangle = CesiumGeospatial::GeographicProjection::MAXIMUM_GLOBE_RECTANGLE;
  uint32_t rootTilesX = 2;

  CesiumGeospatial::Projection projection;

	// 指定投影坐标系。
	if (this->_options.projection) {
		projection = _options.projection.value();

		// 需要根据投影方式确定 tilingSchemeRectangle 和 rootTilesX
		// CesiumGeospatial::Projection 是一个 std::variant，其模板参数类型索引为 GeographicProjection = 0, WebMercatorProjection = 1
		// 【注意】如果以后 Cesium 修改了此 variant 的参数模板定义，这里也要相应修改！
		if (projection.index() == 0)
		{
			// 地理坐标系
			tilingSchemeRectangle = CesiumGeospatial::GeographicProjection::MAXIMUM_GLOBE_RECTANGLE;
			rootTilesX = 2;
		}
		else if(projection.index() == 1)
		{
			// Web 墨卡托坐标系
			tilingSchemeRectangle = CesiumGeospatial::WebMercatorProjection::MAXIMUM_GLOBE_RECTANGLE;
			rootTilesX = 1;
		}
	} else {
		// 默认使用 4326 坐标系投影
		projection = CesiumGeospatial::GeographicProjection();
		rootTilesX = 2;
	}


	// 瓦片范围矩形
	CesiumGeometry::Rectangle coverageRectangle =
    projectRectangleSimple(projection, tilingSchemeRectangle);

	if (_options.coverageRectangle) {
    coverageRectangle = _options.coverageRectangle.value();
  }


	// 瓦片布局方案
	CesiumGeometry::QuadtreeTilingScheme tilingScheme(
    projectRectangleSimple(projection, tilingSchemeRectangle),
    rootTilesX,
    1);

	if (this->_options.tilingScheme) {
		tilingScheme = this->_options.tilingScheme.value();
	}

	uint32_t tileWidth = _options.tileWidth.value_or(256);
	uint32_t tileHeight = _options.tileHeight.value_or(256);

	uint32_t minimumLevel = _options.minimumLevel.value_or(0);
	uint32_t maximumLevel = _options.maximumLevel.value_or(25);
	int32_t levelBias = _options.levelBias.value_or(0);

	bool flipY = _options.flipY.value_or(false);

	// 不用等待远程访问，直接生成一个已经 Resolve 的 Future。参见 RasterizedPolygonsOverlay.cpp
        return asyncSystem.createResolvedFuture<CreateTileProviderResult>(
            IntrusivePointer<UrlTemplateTileProvider>(
                new UrlTemplateTileProvider(
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
                    _headers,
                    _fileExtension,
                    tileWidth,
                    tileHeight,
                    minimumLevel,
                    maximumLevel,
                    levelBias,
                    flipY)));
}

} // namespace Cesium3DTilesSelection
