package com.eggapp.field

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.Surface
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.core.content.ContextCompat
import androidx.navigation.NavType
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import androidx.navigation.navArgument
import com.eggapp.field.data.TokenStore
import com.eggapp.field.ui.batch.BatchDetailScreen
import com.eggapp.field.ui.batch.BatchesScreen
import com.eggapp.field.ui.collections.CollectionsScreen
import com.eggapp.field.ui.flocks.FlockDetailScreen
import com.eggapp.field.ui.flocks.FlocksScreen
import com.eggapp.field.ui.incubators.IncubatorsScreen
import com.eggapp.field.ui.incubators.SetpointsScreen
import com.eggapp.field.ui.login.LoginScreen
import com.eggapp.field.ui.theme.EggAppTheme

private const val ROUTE_LOGIN = "login"
private const val ROUTE_INCUBATORS = "incubators"
private const val ROUTE_BATCHES = "batches"
private const val ROUTE_BATCH_DETAIL = "batch/{batchId}"
private const val ROUTE_COLLECTIONS = "collections"
private const val ROUTE_SETPOINTS = "incubator/{incubatorId}/setpoints"
private const val ROUTE_FLOCKS = "flocks"
private const val ROUTE_FLOCK_DETAIL = "flock/{flockId}"

class MainActivity : ComponentActivity() {

    private val requestNotificationPermission =
        registerForActivityResult(ActivityResultContracts.RequestPermission()) { /* no-op either way — pushes just won't show if denied */ }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requestNotificationPermissionIfNeeded()

        setContent {
            EggAppTheme {
                Surface(modifier = Modifier.fillMaxSize()) {
                    val navController = rememberNavController()
                    val alreadyLoggedIn = remember(this) { TokenStore(this).accessToken() != null }

                    NavHost(
                        navController = navController,
                        startDestination = if (alreadyLoggedIn) ROUTE_INCUBATORS else ROUTE_LOGIN,
                    ) {
                        composable(ROUTE_LOGIN) {
                            LoginScreen(onLoggedIn = {
                                navController.navigate(ROUTE_INCUBATORS) {
                                    popUpTo(ROUTE_LOGIN) { inclusive = true }
                                }
                            })
                        }
                        composable(ROUTE_INCUBATORS) {
                            IncubatorsScreen(
                                onLogout = {
                                    navController.navigate(ROUTE_LOGIN) {
                                        popUpTo(ROUTE_INCUBATORS) { inclusive = true }
                                    }
                                },
                                onOpenBatches = { navController.navigate(ROUTE_BATCHES) },
                                onOpenCollections = { navController.navigate(ROUTE_COLLECTIONS) },
                                onOpenSetpoints = { incubatorId -> navController.navigate("incubator/$incubatorId/setpoints") },
                                onOpenFlocks = { navController.navigate(ROUTE_FLOCKS) },
                            )
                        }
                        composable(ROUTE_BATCHES) {
                            BatchesScreen(onOpenBatch = { batch -> navController.navigate("batch/${batch.id}") })
                        }
                        composable(ROUTE_COLLECTIONS) {
                            CollectionsScreen(onBack = { navController.popBackStack() })
                        }
                        composable(ROUTE_FLOCKS) {
                            FlocksScreen(onOpenFlock = { flock -> navController.navigate("flock/${flock.id}") })
                        }
                        composable(
                            ROUTE_FLOCK_DETAIL,
                            arguments = listOf(navArgument("flockId") { type = NavType.StringType }),
                        ) { backStackEntry ->
                            val flockId = backStackEntry.arguments?.getString("flockId")!!
                            FlockDetailScreen(flockId = flockId, onBack = { navController.popBackStack() })
                        }
                        composable(
                            ROUTE_SETPOINTS,
                            arguments = listOf(navArgument("incubatorId") { type = NavType.StringType }),
                        ) { backStackEntry ->
                            val incubatorId = backStackEntry.arguments?.getString("incubatorId")!!
                            SetpointsScreen(incubatorId = incubatorId, onBack = { navController.popBackStack() })
                        }
                        composable(
                            ROUTE_BATCH_DETAIL,
                            arguments = listOf(navArgument("batchId") { type = NavType.StringType }),
                        ) { backStackEntry ->
                            val batchId = backStackEntry.arguments?.getString("batchId")!!
                            BatchDetailScreen(batchId = batchId, onBack = { navController.popBackStack() })
                        }
                    }
                }
            }
        }
    }

    // Android 13+ (API 33) requires runtime consent to show notifications
    // at all — without this, FCM messages still arrive but never display.
    private fun requestNotificationPermissionIfNeeded() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU) return
        val granted = ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS) ==
            PackageManager.PERMISSION_GRANTED
        if (!granted) requestNotificationPermission.launch(Manifest.permission.POST_NOTIFICATIONS)
    }
}
