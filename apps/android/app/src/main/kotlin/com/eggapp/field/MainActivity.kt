package com.eggapp.field

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.material3.Surface
import androidx.compose.runtime.remember
import androidx.compose.ui.Modifier
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController
import com.eggapp.field.data.TokenStore
import com.eggapp.field.ui.incubators.IncubatorsScreen
import com.eggapp.field.ui.login.LoginScreen
import com.eggapp.field.ui.theme.EggAppTheme

private const val ROUTE_LOGIN = "login"
private const val ROUTE_INCUBATORS = "incubators"

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
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
                            IncubatorsScreen(onLogout = {
                                navController.navigate(ROUTE_LOGIN) {
                                    popUpTo(ROUTE_INCUBATORS) { inclusive = true }
                                }
                            })
                        }
                    }
                }
            }
        }
    }
}
