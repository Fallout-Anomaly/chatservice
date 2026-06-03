#include "PCH.h"

F4SE_PLUGIN_VERSION = []() noexcept {
	F4SE::PluginVersionData v{};
	v.PluginVersion({ Version::MAJOR, Version::MINOR, Version::PATCH, 0 });
	v.PluginName("FalloutChat");
	v.AuthorName("NomadsReach");
	v.UsesAddressLibrary(true);
	v.UsesAddressLibraryNG(true);
	v.UsesSigScanning(false);
	v.IsLayoutDependent(true);
	v.IsLayoutDependentNG(true);
	v.HasNoStructUse(false);
	return v;
}();
