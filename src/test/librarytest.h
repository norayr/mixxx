#pragma once

#include <memory>

#include "database/mixxxdb.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "test/mixxxdbtest.h"
#include "util/db/dbconnectionpooled.h"
#include "util/db/dbconnectionpooler.h"

class LibraryTest : public MixxxDbTest {
  protected:
    LibraryTest();
    ~LibraryTest() override = default;

    TrackCollectionManager* trackCollections() {
        return m_pTrackCollectionManager.get();
    }

    TrackCollection* internalCollection() {
        return trackCollections()->internalCollection();
    }

    TrackPointer getOrAddTrackByLocation(
            const QString& trackLocation);

  private:
    const std::unique_ptr<TrackCollectionManager> m_pTrackCollectionManager;
    ControlObject m_keyNotationCO;
};
