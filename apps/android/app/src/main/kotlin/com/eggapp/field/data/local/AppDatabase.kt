package com.eggapp.field.data.local

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase

@Database(
    entities = [CandlingEntity::class, HatchEntity::class, CollectionEntity::class],
    version = 2,
    exportSchema = false,
)
abstract class AppDatabase : RoomDatabase() {
    abstract fun fieldRecordDao(): FieldRecordDao

    companion object {
        @Volatile private var instance: AppDatabase? = null

        fun get(context: Context): AppDatabase =
            instance ?: synchronized(this) {
                instance ?: Room.databaseBuilder(
                    context.applicationContext,
                    AppDatabase::class.java,
                    "eggapp-field.db",
                )
                    // No prior release to preserve queued rows across —
                    // safe to drop and recreate on the version 1 -> 2 bump.
                    .fallbackToDestructiveMigration()
                    .build().also { instance = it }
            }
    }
}
