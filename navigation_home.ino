import 'package:flutter/material.dart';
import 'package:firebase_database/firebase_database.dart';
import 'package:flutter_phone_direct_caller/flutter_phone_direct_caller.dart';
import 'package:flutter_tts/flutter_tts.dart';
import 'package:speech_to_text/speech_to_text.dart' as stt;
import 'package:url_launcher/url_launcher.dart';
import 'dart:io'; // For app exit functionality

class NavigationScreen extends StatefulWidget {
  @override
  _NavigationScreenState createState() => _NavigationScreenState();
}

class _NavigationScreenState extends State<NavigationScreen> {
  final DatabaseReference database = FirebaseDatabase.instance.ref();
  final String userId = "userID"; // Replace with actual user ID

  final stt.SpeechToText _speech = stt.SpeechToText();
  final FlutterTts _flutterTts = FlutterTts();

  @override
  void initState() {
    super.initState();
    checkHomeLocation();
  }

  /// 🔹 *Check if Home Location Exists*
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

  /// 🔹 *Ask User to Set Home Location*
  void askToSetHomeLocation() async {
    await _flutterTts.speak("Do you want to set this location as your home?");
    await Future.delayed(Duration(seconds: 3));
    startListeningForUserResponse();
  }

  /// 🔹 *Ask User to Navigate Home*
  void askUserToNavigateHome() async {
    await _flutterTts.speak("Do you want to return home? Say navigate to navigate home or emergency for emergency. Do you want to return to the home page for that say home screen");
    await Future.delayed(Duration(seconds:15));
    startListeningForUserResponse();
  }

  /// 🔹 *Start Listening for 'Yes' or 'Help'*
  void startListeningForUserResponse() async {
    bool available = await _speech.initialize();

    if (available) {
      _speech.listen(
        onResult: (val) async {
          String command = val.recognizedWords.toLowerCase();
          print("🎤 Command Received: $command");

          if (command.contains("navigate")) {
            navigateToHome();
          } else if (command.contains("emergency")) {
            handleEmergency();
          } else if (command.contains("homescreen")) {
            await Future.delayed(Duration(seconds: 2)); // Ensure speech completes
            goback(context);
          }
        },
      );
    }
  }

  /// 🔹 *Navigate to Home in Google Maps & Exit App*
  Future<void> navigateToHome() async {
    DataSnapshot snapshot = await database.child("users/$userId/home_location").get();

    if (snapshot.exists && snapshot.value != null) {
      Map<dynamic, dynamic> data = snapshot.value as Map<dynamic, dynamic>;
      double homeLat = (data['latitude'] as num).toDouble();
      double homeLon = (data['longitude'] as num).toDouble();

      String googleMapsUrl =
          "https://www.google.com/maps/dir/?api=1&destination=$homeLat,$homeLon&travelmode=walking";

      Uri mapsUri = Uri.parse(googleMapsUrl);

      if (await canLaunchUrl(mapsUri)) {
        await _flutterTts.speak("Starting navigation to home.");
        await launchUrl(mapsUri, mode: LaunchMode.externalApplication);
        exitApp(); // 🚀 Close the app after launching Google Maps
      } else {
        print("❌ Could not launch Google Maps.");
        await _flutterTts.speak("Error! Unable to open Google Maps.");
      }
    } else {
      print("❌ No home location found!");
      await _flutterTts.speak("Home location is not set. Please set it first.");
    }
  }

  /// 🔹 *Handle Emergency Call & Exit App*
  void handleEmergency() async {
    await database.child("emergency_status").set({
      "status": "help",
      "timestamp": DateTime.now().toString()
    });

    const String emergencyNumber = "+917592894755";
    bool? res = await FlutterPhoneDirectCaller.callNumber(emergencyNumber);
    if (res == true) {
      print("📞 Emergency call placed successfully!");
      exit(0);
    } else {
      print("❌ Could not place emergency call.");
    }
  }
  void goback(BuildContext context) {
    if (Navigator.canPop(context)) {
      Navigator.pop(context);
    }
  }/// 🔹 *Exit the App*
  void exitApp() {
    Future.delayed(Duration(seconds: 3), () {
      exit(0); // 🚀 Force quit the app
    });
  }

    @override
    Widget build(BuildContext context) {
      return Scaffold(
        appBar: AppBar(title: Text("Navigation Screen")),
        body: Center(child: Text("Fetching location...")),
      );
    }

  }