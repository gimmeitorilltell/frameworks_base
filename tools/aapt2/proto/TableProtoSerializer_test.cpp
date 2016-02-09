/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ResourceTable.h"
#include "proto/ProtoSerialize.h"
#include "test/Builders.h"
#include "test/Common.h"
#include "test/Context.h"

#include <gtest/gtest.h>

namespace aapt {

TEST(TableProtoSerializer, SerializeSinglePackage) {
    std::unique_ptr<IAaptContext> context = test::ContextBuilder().build();
    std::unique_ptr<ResourceTable> table = test::ResourceTableBuilder()
            .setPackageId(u"com.app.a", 0x7f)
            .addFileReference(u"@com.app.a:layout/main", ResourceId(0x7f020000),
                              u"res/layout/main.xml")
            .addReference(u"@com.app.a:layout/other", ResourceId(0x7f020001),
                          u"@com.app.a:layout/main")
            .addString(u"@com.app.a:string/text", {}, u"hi")
            .addValue(u"@com.app.a:id/foo", {}, util::make_unique<Id>())
            .build();

    Symbol publicSymbol;
    publicSymbol.state = SymbolState::kPublic;
    ASSERT_TRUE(table->setSymbolState(test::parseNameOrDie(u"@com.app.a:layout/main"),
                                      ResourceId(0x7f020000),
                                      publicSymbol, context->getDiagnostics()));

    Id* id = test::getValue<Id>(table.get(), u"@com.app.a:id/foo");
    ASSERT_NE(nullptr, id);

    // Make a plural.
    std::unique_ptr<Plural> plural = util::make_unique<Plural>();
    plural->values[Plural::One] = util::make_unique<String>(table->stringPool.makeRef(u"one"));
    ASSERT_TRUE(table->addResource(test::parseNameOrDie(u"@com.app.a:plurals/hey"),
                                   ConfigDescription{}, std::move(plural),
                                   context->getDiagnostics()));

    std::unique_ptr<pb::ResourceTable> pbTable = serializeTableToPb(table.get());
    ASSERT_NE(nullptr, pbTable);

    std::unique_ptr<ResourceTable> newTable = deserializeTableFromPb(*pbTable,
                                                                     Source{ "test" },
                                                                     context->getDiagnostics());
    ASSERT_NE(nullptr, newTable);

    Id* newId = test::getValue<Id>(newTable.get(), u"@com.app.a:id/foo");
    ASSERT_NE(nullptr, newId);
    EXPECT_EQ(id->isWeak(), newId->isWeak());

    Maybe<ResourceTable::SearchResult> result = newTable->findResource(
            test::parseNameOrDie(u"@com.app.a:layout/main"));
    AAPT_ASSERT_TRUE(result);
    EXPECT_EQ(SymbolState::kPublic, result.value().type->symbolStatus.state);
    EXPECT_EQ(SymbolState::kPublic, result.value().entry->symbolStatus.state);
}

TEST(TableProtoSerializer, SerializeFileHeader) {
    std::unique_ptr<IAaptContext> context = test::ContextBuilder().build();

    ResourceFile f;
    f.config = test::parseConfigOrDie("hdpi-v9");
    f.name = test::parseNameOrDie(u"@com.app.a:layout/main");
    f.source.path = "res/layout-hdpi-v9/main.xml";
    f.exportedSymbols.push_back(SourcedResourceName{ test::parseNameOrDie(u"@+id/unchecked"), 23u });

    const std::string expectedData = "1234";

    std::unique_ptr<pb::CompiledFile> pbFile = serializeCompiledFileToPb(f);

    std::string outputStr;
    {
        google::protobuf::io::StringOutputStream outStream(&outputStr);
        CompiledFileOutputStream outFileStream(&outStream, pbFile.get());

        ASSERT_TRUE(outFileStream.Write(expectedData.data(), expectedData.size()));
        ASSERT_TRUE(outFileStream.Finish());
    }

    CompiledFileInputStream inFileStream(outputStr.data(), outputStr.size());
    const pb::CompiledFile* newPbFile = inFileStream.CompiledFile();
    ASSERT_NE(nullptr, newPbFile);

    std::unique_ptr<ResourceFile> file = deserializeCompiledFileFromPb(*newPbFile, Source{ "test" },
                                                                       context->getDiagnostics());
    ASSERT_NE(nullptr, file);

    std::string actualData((const char*)inFileStream.data(), inFileStream.size());
    EXPECT_EQ(expectedData, actualData);
    EXPECT_EQ(0u, reinterpret_cast<uintptr_t>(inFileStream.data()) & 0x03);

    ASSERT_EQ(1u, file->exportedSymbols.size());
    EXPECT_EQ(test::parseNameOrDie(u"@+id/unchecked"), file->exportedSymbols[0].name);
}

} // namespace aapt
