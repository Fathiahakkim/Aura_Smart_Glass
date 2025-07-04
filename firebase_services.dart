import 'package:flutter/material.dart';
import 'package:firebase_database/firebase_database.dart';
import 'package:flutter_tts/flutter_tts.dart';
import 'package:speech_to_text/speech_to_text.dart' as stt;
import 'package:url_launcher/url_launcher.dart';

class NavigationScreen extends StatefulWidget {
  @override
  _NavigationScreenState createState() => _NavigationScreenState();
}

class _NavigationScreenState extends State<NavigationScreen> {
  final DatabaseReference database = FirebaseDatabase.instance.ref();
  final String userId = "userID"; // Replace with actual user ID

  final stt.SpeechToText _speech = stt.SpeechToText();
  final FlutterTts _flutterTts = FlutterTts();

  bool _isListening = false;

  @override
  void initState() {
    super.initState();
    checkHomeLocation();
  }

  /// 🔹 **Check if Home Location Exists**
  void checkHomeLocation() async {
    DataSnapshot snapshot = await database.child("users/$userId/home_location").get();

    if (snapshot.exists && snapshot.value != null) {
      print("✅ Home location already set.");
      askUserToNavigateHome();
    } else {
      print("❌ Home location not set. Asking user to set it.");
      askToSetHomeLocation();
    }
  }

  /// 🔹 **Fetch Current Location from Firebase `/gps`**
  Future<Map<String, double>?> fetchCurrentLocation() async {
    try {
      DataSnapshot snapshot = await database.child("gps").get();

      if (snapshot.exists && snapshot.value != null) {
        Map<dynamic, dynamic> data = snapshot.value as Map<dynamic, dynamic>;
        double lat = (data['latitude'] as num).toDouble();
        double lon = (data['longitude'] as num).toDouble();
        return {"latitude": lat, "longitude": lon};
      } else {
        print("❌ No GPS data found!");
        return null;
      }
    } catch (e) {
      print("❌ Error fetching current location: $e");
      return null;
    }
  }

  /// 🔹 **Ask User to Set Home Location**
  void askToSetHomeLocation() async {
    await _flutterTts.speak("Do you want to set this location as your home?");
    await Future.delayed(Duration(seconds: 3));
    startListeningForHomeSetup();
  }

  void startListeningForHomeSetup() async {
    if (!_isListening) {
      bool available = await _speech.initialize();

      if (available) {
        setState(() => _isListening = true);
        _speech.listen(
          onResult: (val) async {
            String command = val.recognizedWords.toLowerCase();
            if (command.contains("yes")) {
              await saveHomeLocation();
              askToStayOrHomeScreen(() => askToSetHomeLocation());
            } else {
              repeatQuestion(() => askToSetHomeLocation());
            }
            setState(() => _isListening = false);
          },
          listenFor: Duration(seconds: 15),
          partialResults: true,
        );
      }
    }
  }

  /// 🔹 **Save Current Location as Home**
  Future<void> saveHomeLocation() async {
    Map<String, double>? currentLoc = await fetchCurrentLocation();
    if (currentLoc != null) {
      await database.child("users/$userId/home_location").set({
        "latitude": currentLoc['latitude'],
        "longitude": currentLoc['longitude'],
      });
      await _flutterTts.speak("Your home location has been saved.");
    } else {
      print("❌ Error saving home location.");
    }
  }

  /// 🔹 **Ask User to Navigate Home**
  void askUserToNavigateHome() async {
    await _flutterTts.speak("Do you want to return home?yes or Say help if you need emergency assistance.");
    await Future.delayed(Duration(seconds: 10));
    startListeningForNavigation();
  }

  void startListeningForNavigation() async {
    if (!_isListening) {
      bool available = await _speech.initialize();

      if (available) {
        setState(() => _isListening = true);

        _speech.listen(
          onResult: (val) async {
            String command = val.recognizedWords.toLowerCase().trim();

            print("🔍 Detected command: $command");

            if (command == "yes") {
              _speech.stop(); // Stop listening before navigating
              navigateToHome();
            } else if (command == "help") {
              _speech.stop(); // Stop listening before handling emergency
              handleEmergency();
            } else {
              repeatQuestion(() => askUserToNavigateHome());
            }

            setState(() => _isListening = false);
          },
          listenFor: Duration(seconds: 90),
          partialResults: false, // ✅ Only accept final results
        );
      }
    }
  }


  /// 🔹 **Handle Emergency Call**
  void handleEmergency() async {
    _speech.stop(); // ✅ Stop listening to avoid multiple actions

    await database.child("emergency_status").set({
      "status": "help",
      "timestamp": DateTime.now().toString()
    });
    makeEmergencyCall();
    Future.delayed(Duration(seconds: 120), () async {
      await database.child("emergency_status").remove();
    });
  }

  /// 🔹 **Make an Emergency Call**
  Future<void> makeEmergencyCall() async {
    const String emergencyNumber = "+919995351072"; // Change this to any emergency number
    Uri callUri = Uri.parse("tel:$emergencyNumber");

    if (await canLaunchUrl(callUri)) {
      await launchUrl(callUri);
    } else {
      print("❌ Could not place emergency call.");
    }
  }

  /// 🔹 **Navigate to Home in Google Maps**
  Future<void> navigateToHome() async {
    _speech.stop(); // ✅ Stop listening before navigation starts

    DataSnapshot snapshot = await database.child("users/$userId/home_location").get();

    if (snapshot.exists && snapshot.value != null) {
      Map<dynamic, dynamic> data = snapshot.value as Map<dynamic, dynamic>;
      double homeLat = (data['latitude'] as num).toDouble();
      double homeLon = (data['longitude'] as num).toDouble();

      Map<String, double>? currentLoc = await fetchCurrentLocation();
      if (currentLoc == null) {
        print("❌ Missing current location!");
        await _flutterTts.speak("Error! Could not fetch your current location.");
        return;
      }

      // 🔹 Use Google Maps walking mode with voice guidance
      String googleMapsUrl =
          "https://www.google.com/maps/dir/?api=1&destination=$homeLat,$homeLon&travelmode=walking&dir_action=navigate";

      Uri mapsUri = Uri.parse(googleMapsUrl);

      if (await canLaunchUrl(mapsUri)) {
        await _flutterTts.speak("Starting navigation to your home location.");
        await launchUrl(mapsUri, mode: LaunchMode.externalApplication);
        print("🚶‍♂️ Walking navigation started with voice guidance!");
      } else {
        print("❌ Could not launch Google Maps.");
        await _flutterTts.speak("Error! Unable to open Google Maps.");
      }
    } else {
      print("❌ No home location found!");
      await _flutterTts.speak("Home location is not set. Please set it first.");
    }
  }


  /// 🔹 **Ask User to Stay or Return to Home Screen**
  void askToStayOrHomeScreen(Function retryAction) async {
    await _flutterTts.speak("Do you want to stay or go back to the home screen?");
    await Future.delayed(Duration(seconds: 3));
    listenForStayOrHomeScreen(retryAction);
  }

  void listenForStayOrHomeScreen(Function retryAction) async {
    if (!_isListening) {
      bool available = await _speech.initialize();

      if (available) {
        setState(() => _isListening = true);
        _speech.listen(
          onResult: (val) async {
            String command = val.recognizedWords.toLowerCase();
            if (command.contains("stay")) {
              print("✅ User chose to stay.");
            } else if (command.contains("home")) {
              Navigator.pop(context);
            } else {
              repeatQuestion(() => askToStayOrHomeScreen(retryAction));
            }
            setState(() => _isListening = false);
          },
          listenFor: Duration(seconds: 95),
          partialResults: true,
        );
      }
    }
  }

  /// 🔹 **Repeat Question if No Response**
  void repeatQuestion(Function retryAction) async {
    await _flutterTts.speak("I didn't hear you. Please answer again.");
    await Future.delayed(Duration(seconds: 2));
    _speech.stop(); // ✅ Ensure previous listening session is stopped

    retryAction();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: Text("Navigation Screen")),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Text("Fetching location..."),
          ],
        ),
      ),
    );
  }
}
