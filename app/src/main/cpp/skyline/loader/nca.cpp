// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include <vfs/npdm.h>
#include "nso.h"
#include "nca.h"

namespace skyline::loader {
    NcaLoader::NcaLoader(const std::shared_ptr<vfs::Backing> &backing, const std::shared_ptr<crypto::KeyStore> &keyStore) : nca(backing, keyStore) {
        if (nca.exeFs == nullptr)
            throw exception("Only NCAs with an ExeFS can be loaded directly");
    }

    void *NcaLoader::LoadExeFs(const std::shared_ptr<vfs::FileSystem> &exeFs, const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state) {
        if (exeFs == nullptr)
            throw exception("Cannot load a null ExeFS");

        auto nsoFile{exeFs->OpenFile("rtld")};
        if (nsoFile == nullptr)
            throw exception("Cannot load an ExeFS that doesn't contain rtld");

        state.process->memory.InitializeVmm(process->npdm.meta.flags.type);

        auto loadInfo{NsoLoader::LoadNso(nsoFile, process, state)};
        u64 offset{loadInfo.size};
        u8 *base{loadInfo.base};
        void *entry{loadInfo.entry};

        state.logger->Info("Loaded 'rtld.nso' at 0x{:X} (.text @ 0x{:X})", base, entry);

        for (const auto &nso : {"main", "subsdk0", "subsdk1", "subsdk2", "subsdk3", "subsdk4", "subsdk5", "subsdk6", "subsdk7", "sdk"}) {
            if (exeFs->FileExists(nso))
                nsoFile = exeFs->OpenFile(nso);
            else
                continue;

            loadInfo = NsoLoader::LoadNso(nsoFile, process, state, offset);
            state.logger->Info("Loaded '{}.nso' at 0x{:X} (.text @ 0x{:X})", nso, base + offset, loadInfo.entry);
            offset += loadInfo.size;
        }

        state.process->memory.InitializeRegions(base, offset);

        return entry;
    }

    void *NcaLoader::LoadProcessData(const std::shared_ptr<kernel::type::KProcess> process, const DeviceState &state) {
        process->npdm = vfs::NPDM(nca.exeFs->OpenFile("main.npdm"), state);
        return LoadExeFs(nca.exeFs, process, state);
    }
}
