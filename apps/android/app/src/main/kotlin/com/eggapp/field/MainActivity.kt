package com.eggapp.field

import android.Manifest
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Egg
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Pets
import androidx.compose.material.icons.filled.Person
import androidx.compose.material3.Icon
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.core.content.ContextCompat
import androidx.navigation.NavType
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
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
import com.eggapp.field.ui.inventory.InventoryScreen
import com.eggapp.field.ui.login.LoginScreen
import com.eggapp.field.ui.profile.ProfileScreen
import com.eggapp.field.ui.theme.EggAppTheme
import com.eggapp.field.ui.vaccination.VaccinationTemplatesScreen

private const val ROUTE_LOGIN = "login"
private const val ROUTE_INCUBATORS = "incubators"
private const val ROUTE_BATCHES = "batches"
private const val ROUTE_BATCH_DETAIL = "batch/{batchId}"
private const val ROUTE_COLLECTIONS = "collections"
private const val ROUTE_SETPOINTS = "incubator/{incubatorId}/setpoints"
private const val ROUTE_FLOCKS = "flocks"
private const val ROUTE_FLOCK_DETAIL = "flock/{flockId}"
private const val ROUTE_PROFILE = "profile"
private const val ROUTE_VACCINATION_TEMPLATES = "vaccination-templates"
private const val ROUTE_INVENTORY = "inventory"

// Bottom tabs — mirrors the eggAPP web sidebar's top-level sections, minus
// the admin-only ones (Devices/Alerts stay web-only per CLAUDE.md's surface
// split). Collections is reached from the Home tab rather than getting its
// own slot — four tabs is plenty for one thumb. Vaccination templates and
// Inventory (Phase 4 — full parity with web) are reached from Profile
// rather than a fifth/sixth tab, for the same reason.
private data class Tab(val route: String, val label: String, val icon: ImageVector)

private val TABS = listOf(
    Tab(ROUTE_INCUBATORS, "Home", Icons.Filled.Home),
    Tab(ROUTE_FLOCKS, "Flocks", Icons.Filled.Pets),
    Tab(ROUTE_BATCHES, "Batches", Icons.Filled.Egg),
    Tab(ROUTE_PROFILE, "Profile", Icons.Filled.Person),
)

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
                    val backStackEntry by navController.currentBackStackEntryAsState()
                    val currentRoute = backStackEntry?.destination?.route
                    val showBottomBar = TABS.any { it.route == currentRoute }

                    Scaffold(
                        bottomBar = {
                            if (showBottomBar) {
                                NavigationBar {
                                    TABS.forEach { tab ->
                                        NavigationBarItem(
                                            selected = currentRoute == tab.route,
                                            onClick = {
                                                if (currentRoute != tab.route) {
                                                    navController.navigate(tab.route) {
                                                        popUpTo(navController.graph.startDestinationId) { saveState = true }
                                                        launchSingleTop = true
                                                        restoreState = true
                                                    }
                                                }
                                            },
                                            icon = { Icon(tab.icon, contentDescription = tab.label) },
                                            label = { Text(tab.label) },
                                        )
                                    }
                                }
                            }
                        },
                    ) { padding ->
                        NavHost(
                            navController = navController,
                            startDestination = if (alreadyLoggedIn) ROUTE_INCUBATORS else ROUTE_LOGIN,
                            modifier = Modifier.padding(bottom = padding.calculateBottomPadding()),
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
                                    onOpenCollections = { navController.navigate(ROUTE_COLLECTIONS) },
                                    onOpenSetpoints = { incubatorId -> navController.navigate("incubator/$incubatorId/setpoints") },
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
                            composable(ROUTE_PROFILE) {
                                ProfileScreen(
                                    onLoggedOut = {
                                        navController.navigate(ROUTE_LOGIN) {
                                            popUpTo(0) { inclusive = true }
                                        }
                                    },
                                    onOpenVaccinationTemplates = { navController.navigate(ROUTE_VACCINATION_TEMPLATES) },
                                    onOpenInventory = { navController.navigate(ROUTE_INVENTORY) },
                                )
                            }
                            composable(ROUTE_VACCINATION_TEMPLATES) {
                                VaccinationTemplatesScreen(onBack = { navController.popBackStack() })
                            }
                            composable(ROUTE_INVENTORY) {
                                InventoryScreen(onBack = { navController.popBackStack() })
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
