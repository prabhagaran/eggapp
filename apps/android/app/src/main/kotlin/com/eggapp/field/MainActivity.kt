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
import androidx.compose.material.icons.filled.NotificationsActive
import androidx.compose.material.icons.filled.Egg
import androidx.compose.material.icons.filled.Inventory
import androidx.compose.material.icons.filled.Inventory2
import androidx.compose.material.icons.filled.Person
import androidx.compose.material.icons.filled.Pets
import androidx.compose.material.icons.filled.SpaceDashboard
import androidx.compose.material.icons.filled.Vaccines
import androidx.compose.material.icons.filled.Thermostat
import androidx.compose.material3.Icon
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.core.content.ContextCompat
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation.NavHostController
import androidx.navigation.NavType
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import androidx.navigation.navArgument
import com.eggapp.field.data.TokenStore
import com.eggapp.field.ui.alerts.AlertsScreen
import com.eggapp.field.ui.batch.BatchDetailScreen
import com.eggapp.field.ui.batch.BatchesScreen
import com.eggapp.field.ui.collections.CollectionsScreen
import com.eggapp.field.ui.components.EggAppTopBar
import com.eggapp.field.ui.components.OverflowItem
import com.eggapp.field.ui.dashboard.DashboardScreen
import com.eggapp.field.ui.dashboard.DashboardViewModel
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
private const val ROUTE_DASHBOARD = "dashboard"
private const val ROUTE_INCUBATORS = "incubators"
private const val ROUTE_BATCHES = "batches"
private const val ROUTE_BATCH_DETAIL = "batch/{batchId}"
private const val ROUTE_COLLECTIONS = "collections"
private const val ROUTE_SETPOINTS = "incubator/{incubatorId}/setpoints"
private const val ROUTE_FLOCKS = "flocks"
private const val ROUTE_FLOCK_DETAIL = "flock/{flockId}"
private const val ROUTE_PROFILE = "profile"
private const val ROUTE_ALERTS = "alerts"
private const val ROUTE_VACCINATION_TEMPLATES = "vaccination-templates"
private const val ROUTE_INVENTORY = "inventory"

// Bottom tabs — the four destinations a field worker moves between all day.
// Everything else (Collections, Vaccination, Inventory, Alerts, Profile)
// lives in the top bar's kebab, exactly like the web header's "More" menu
// once the tab row stops fitting. Alert *management* stays web-only per
// CLAUDE.md's surface split; the phone only reads them.
private data class Tab(val route: String, val label: String, val icon: ImageVector)

private val TABS = listOf(
    Tab(ROUTE_DASHBOARD, "Dashboard", Icons.Filled.SpaceDashboard),
    Tab(ROUTE_INCUBATORS, "Incubators", Icons.Filled.Thermostat),
    Tab(ROUTE_FLOCKS, "Flocks", Icons.Filled.Pets),
    Tab(ROUTE_BATCHES, "Batches", Icons.Filled.Egg),
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
                    AppScaffold(navController = navController, alreadyLoggedIn = alreadyLoggedIn)
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

@Composable
private fun AppScaffold(navController: NavHostController, alreadyLoggedIn: Boolean) {
    // Hoisted to the activity so the top bar's farm switcher, the kebab's
    // alert badge and the dashboard all read the same poll — three separate
    // ViewModels would mean three copies of the same 15s request.
    val dashboardViewModel: DashboardViewModel = viewModel()
    val dashboard by dashboardViewModel.state.collectAsState()

    val backStackEntry by navController.currentBackStackEntryAsState()
    val currentRoute = backStackEntry?.destination?.route
    val isTabRoute = TABS.any { it.route == currentRoute }

    fun go(route: String) {
        if (currentRoute == route) return
        navController.navigate(route) {
            popUpTo(navController.graph.startDestinationId) { saveState = true }
            launchSingleTop = true
            restoreState = true
        }
    }

    val overflow = listOf(
        OverflowItem("Alerts", Icons.Filled.NotificationsActive, badge = dashboard.alerts.size) { navController.navigate(ROUTE_ALERTS) },
        OverflowItem("Egg collections", Icons.Filled.Inventory2) { navController.navigate(ROUTE_COLLECTIONS) },
        OverflowItem("Vaccination templates", Icons.Filled.Vaccines) { navController.navigate(ROUTE_VACCINATION_TEMPLATES) },
        OverflowItem("Inventory", Icons.Filled.Inventory) { navController.navigate(ROUTE_INVENTORY) },
        OverflowItem("Profile", Icons.Filled.Person) { navController.navigate(ROUTE_PROFILE) },
    )

    Scaffold(
        topBar = {
            if (isTabRoute) {
                EggAppTopBar(
                    farmName = dashboard.farmName,
                    farms = dashboard.farms.map { it.id to it.name },
                    selectedFarmId = dashboard.farmId,
                    onSelectFarm = { id ->
                        dashboardViewModel.selectFarm(id)
                        // Every ViewModel reads farmId once on construction,
                        // so a client-side nav would leave already-created
                        // screens pointed at the old farm. Recreating the
                        // activity is the phone equivalent of the web's
                        // full reload on farm switch.
                        (navController.context as? android.app.Activity)?.recreate()
                    },
                    userLabel = initials(dashboard.me?.name, dashboard.me?.email),
                    overflow = overflow,
                )
            }
        },
        bottomBar = {
            if (isTabRoute) {
                NavigationBar {
                    TABS.forEach { tab ->
                        NavigationBarItem(
                            selected = currentRoute == tab.route,
                            onClick = { go(tab.route) },
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
            startDestination = if (alreadyLoggedIn) ROUTE_DASHBOARD else ROUTE_LOGIN,
            modifier = Modifier.padding(
                top = padding.calculateTopPadding(),
                bottom = padding.calculateBottomPadding(),
            ),
        ) {
            composable(ROUTE_LOGIN) {
                LoginScreen(onLoggedIn = {
                    navController.navigate(ROUTE_DASHBOARD) {
                        popUpTo(ROUTE_LOGIN) { inclusive = true }
                    }
                })
            }
            composable(ROUTE_DASHBOARD) {
                DashboardScreen(
                    viewModel = dashboardViewModel,
                    onOpenAlerts = { navController.navigate(ROUTE_ALERTS) },
                    onOpenBatch = { batch -> navController.navigate("batch/${batch.id}") },
                    onOpenBatches = { go(ROUTE_BATCHES) },
                    onOpenFlocks = { go(ROUTE_FLOCKS) },
                    onOpenIncubators = { go(ROUTE_INCUBATORS) },
                )
            }
            composable(ROUTE_ALERTS) {
                AlertsScreen(alerts = dashboard.alerts, onBack = { navController.popBackStack() })
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
                    onBack = { navController.popBackStack() },
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

private fun initials(name: String?, email: String?): String {
    val source = name?.trim()?.takeIf { it.isNotEmpty() } ?: email ?: return "?"
    val parts = source.split(' ', '@', '.').filter { it.isNotBlank() }
    val letters = (parts.getOrNull(0)?.take(1) ?: "") + (parts.getOrNull(1)?.take(1) ?: "")
    return letters.uppercase().ifEmpty { "?" }
}
