Getting Started
===============

.. _installation:

Installation
------------

Prerequisites
~~~~~~~~~~~~~

Before installing, ensure the following tools are available:

- `PlatformIO <https://platformio.org/>`_ (VS Code extension or CLI)
- A `Spotify Developer App <https://developer.spotify.com/dashboard>`_

Clone and Configure
~~~~~~~~~~~~~~~~~~~

Clone the repository and move into the project directory:

.. code-block:: bash

   git clone git@github.com:Edward-Serna/ELECROW_3.2_ESP32_SPOTIFY_CONTROL.git
   cd ELECROW_3.2_ESP32_APP

Create the file ``src/secrets.h`` with the following contents:

.. code-block:: cpp

   #pragma once
   #define WIFI_SSID        "your_wifi_name"
   #define WIFI_PASS        "your_wifi_password"
   #define SPOTIFY_CLIENT_ID      "your_client_id"
   #define SPOTIFY_CLIENT_SECRET  "your_client_secret"

Configure TFT_eSPI
~~~~~~~~~~~~~~~~~~

Copy the correct ``User_Setup.h`` for the **Elecrow 3.2" board** into the
``TFT_eSPI`` library folder, or enable ``USER_SETUP_LOADED`` inside
``platformio.ini``.

This board uses the **ILI9488 display driver**.

Add Redirect URI
~~~~~~~~~~~~~~~~

In the Spotify Developer Dashboard, add the following redirect URI:

::

   http://<ESP32_IP_ADDRESS>/callback

The ESP32 IP address will be displayed on the boot screen after the device
connects to WiFi.

Build and Flash
~~~~~~~~~~~~~~~

Upload the firmware using PlatformIO:

.. code-block:: bash

   pio run --target upload

Alternatively, use the **PlatformIO sidebar in VS Code → Upload**.

First Boot
----------

1. The device connects to WiFi and displays its IP address.
2. Open ``http://<ESP32_IP>`` in a browser on the same network.
3. Click **Authorize with Spotify** and log in.
4. The device stores the refresh token and loads the player.  
   Authorization is only required once.

Touch Calibration
-----------------

If the touch buttons are misaligned, enable calibration mode in ``config.h``:

.. code-block:: cpp

   #define TOUCH_CAL_MODE 1

Flash the firmware, then tap each corner in the following order:

- Top Left
- Top Right
- Bottom Left
- Bottom Right

Read the raw values from the **serial monitor at 115200 baud**.

Update these constants in ``config.h``:

.. code-block:: cpp

   #define TOUCH_X_MIN   150   // rawX at screen right edge
   #define TOUCH_X_MAX  3880   // rawX at screen left edge (inverted)
   #define TOUCH_Y_MIN   320   // rawY at screen bottom
   #define TOUCH_Y_MAX  3880   // rawY at screen top (inverted)

After updating the values, set calibration mode back to:

.. code-block:: cpp

   #define TOUCH_CAL_MODE 0

Reflash the device.
