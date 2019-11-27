// pages/now_order/now_order.js
var util = require('../../utils/util.js');
import { Config } from '../../utils/config.js';
var latitude = 0;
var longitude = 0;
Page({
  data: {
    order: {},
    img: '',
    coor: {},
    wxlatitude: 0,
    wxlongitude: 0,
    flag: 0
  },

  onLoad: function (options) {
    console.log('订单详情页面');
    this.getLtransImg();
    this.gettime();
    var that = this;
    wx.request({
      url: Config.restUrl + '/api/v1/user/getorderdetail?oid=' + options.oid, // 仅为示例，并非真实的接口地址
      header: {
        'content-type': 'application/json' // 默认值
      },
      success(res) {
        var dataxx = res.data;
        that.setData2page(dataxx)
      }
    })
  },

  setData2page: function (data) {
    console.log(data);
    //console.log(data.car.car_position_wd);
    //console.log(data.car.car_position_jd);
    latitude = Number(data.car.car_position_wd);
    longitude = Number(data.car.car_position_jd);
    this.setData({
      order: data
    });
  },

  gettime: function () {
    var time = util.formatTime(new Date());
    console.log(time);
  },

  getLtransImg: function () {
    var that = this;
    wx.request({
      url: Config.restUrl + '/api/v1/mix/getimg/2',
      data: '',
      header: {
        'content-type': 'application/json' // 默认值
      },
      method: 'get',
      success(res) {
        //console.log(res.data)
        var dataxx = Config.restUrl + '/img' + res.data;
        //console.log("赋值完成")
        that.setData({
          img: dataxx
        });
      }
    })
  },

  pointToLook: function () {
      
      console.log(latitude);
      console.log(longitude);
      wx.openLocation({
        latitude,
        longitude,
        scale: 10
      })
  },

  


  /**
   * 生命周期函数--监听页面初次渲染完成
   */
  onReady: function () {

  },

  /**
   * 生命周期函数--监听页面显示
   */
  onShow: function () {

  },

  /**
   * 生命周期函数--监听页面隐藏
   */
  onHide: function () {

  },

  /**
   * 生命周期函数--监听页面卸载
   */
  onUnload: function () {

  },

  /**
   * 页面相关事件处理函数--监听用户下拉动作
   */
  onPullDownRefresh: function () {

  },

  /**
   * 页面上拉触底事件的处理函数
   */
  onReachBottom: function () {

  },

  /**
   * 用户点击右上角分享
   */
  onShareAppMessage: function () {

  }
})